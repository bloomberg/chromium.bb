// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_vsync_provider.h"

#include <string>

#include "base/atomicops.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"

#include <windows.h>

namespace gpu {

namespace {
// from <D3dkmthk.h>
typedef LONG NTSTATUS;
typedef UINT D3DKMT_HANDLE;
typedef UINT D3DDDI_VIDEO_PRESENT_SOURCE_ID;

#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

typedef struct _D3DKMT_OPENADAPTERFROMHDC {
  HDC hDc;
  D3DKMT_HANDLE hAdapter;
  LUID AdapterLuid;
  D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
} D3DKMT_OPENADAPTERFROMHDC;

typedef struct _D3DKMT_CLOSEADAPTER {
  D3DKMT_HANDLE hAdapter;
} D3DKMT_CLOSEADAPTER;

typedef struct _D3DKMT_WAITFORVERTICALBLANKEVENT {
  D3DKMT_HANDLE hAdapter;
  D3DKMT_HANDLE hDevice;
  D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
} D3DKMT_WAITFORVERTICALBLANKEVENT;

typedef NTSTATUS(APIENTRY* PFND3DKMTOPENADAPTERFROMHDC)(
    D3DKMT_OPENADAPTERFROMHDC*);
typedef NTSTATUS(APIENTRY* PFND3DKMTCLOSEADAPTER)(D3DKMT_CLOSEADAPTER*);
typedef NTSTATUS(APIENTRY* PFND3DKMTWAITFORVERTICALBLANKEVENT)(
    D3DKMT_WAITFORVERTICALBLANKEVENT*);
}  // namespace

// The actual implementation of background tasks plus any state that might be
// needed on the worker thread.
class GpuVSyncWorker : public base::Thread {
 public:
  GpuVSyncWorker(const GpuVSyncProvider::VSyncCallback& callback,
                 SurfaceHandle surface_handle);
  ~GpuVSyncWorker() override;

  void Enable(bool enabled);
  void StartRunningVSyncOnThread();
  void WaitForVSyncOnThread();
  void SendVSyncUpdate(base::TimeTicks timestamp);

 private:
  void Reschedule();
  void OpenAdapter(const wchar_t* device_name);
  void CloseAdapter();
  bool WaitForVBlankEvent();

  // Specifies whether background tasks are running.
  // This can be set on background thread only.
  bool running_ = false;

  // Specified whether the worker is enabled.  This is accessed from both
  // threads but can be changed on the main thread only.
  base::subtle::AtomicWord enabled_ = false;

  const GpuVSyncProvider::VSyncCallback callback_;
  const SurfaceHandle surface_handle_;

  PFND3DKMTOPENADAPTERFROMHDC open_adapter_from_hdc_ptr_;
  PFND3DKMTCLOSEADAPTER close_adapter_ptr_;
  PFND3DKMTWAITFORVERTICALBLANKEVENT wait_for_vertical_blank_event_ptr_;

  std::wstring current_device_name_;
  D3DKMT_HANDLE current_adapter_handle_ = 0;
  D3DDDI_VIDEO_PRESENT_SOURCE_ID current_source_id_ = 0;
};

GpuVSyncWorker::GpuVSyncWorker(const GpuVSyncProvider::VSyncCallback& callback,
                               SurfaceHandle surface_handle)
    : base::Thread(base::StringPrintf("VSync-%d", surface_handle)),
      callback_(callback),
      surface_handle_(surface_handle) {
  HMODULE gdi32 = GetModuleHandle(L"gdi32");
  if (!gdi32) {
    NOTREACHED() << "Can't open gdi32.dll";
    return;
  }

  open_adapter_from_hdc_ptr_ = reinterpret_cast<PFND3DKMTOPENADAPTERFROMHDC>(
      ::GetProcAddress(gdi32, "D3DKMTOpenAdapterFromHdc"));
  if (!open_adapter_from_hdc_ptr_) {
    NOTREACHED() << "Can't find D3DKMTOpenAdapterFromHdc in gdi32.dll";
    return;
  }

  close_adapter_ptr_ = reinterpret_cast<PFND3DKMTCLOSEADAPTER>(
      ::GetProcAddress(gdi32, "D3DKMTCloseAdapter"));
  if (!close_adapter_ptr_) {
    NOTREACHED() << "Can't find D3DKMTCloseAdapter in gdi32.dll";
    return;
  }

  wait_for_vertical_blank_event_ptr_ =
      reinterpret_cast<PFND3DKMTWAITFORVERTICALBLANKEVENT>(
          ::GetProcAddress(gdi32, "D3DKMTWaitForVerticalBlankEvent"));
  if (!wait_for_vertical_blank_event_ptr_) {
    NOTREACHED() << "Can't find D3DKMTWaitForVerticalBlankEvent in gdi32.dll";
    return;
  }
}

GpuVSyncWorker::~GpuVSyncWorker() {
  // Thread::Close() call below will block until this task has finished running
  // so it is safe to post it here and pass unretained pointer.
  task_runner()->PostTask(FROM_HERE, base::Bind(&GpuVSyncWorker::CloseAdapter,
                                                base::Unretained(this)));
  Stop();

  DCHECK_EQ(0u, current_adapter_handle_);
  DCHECK(current_device_name_.empty());
}

void GpuVSyncWorker::Enable(bool enabled) {
  auto was_enabled = base::subtle::NoBarrier_AtomicExchange(&enabled_, enabled);

  if (enabled && !was_enabled)
    task_runner()->PostTask(
        FROM_HERE, base::Bind(&GpuVSyncWorker::StartRunningVSyncOnThread,
                              base::Unretained(this)));
}

void GpuVSyncWorker::StartRunningVSyncOnThread() {
  DCHECK(base::PlatformThread::CurrentId() == GetThreadId());

  if (!running_) {
    running_ = true;
    WaitForVSyncOnThread();
  }
}

void GpuVSyncWorker::WaitForVSyncOnThread() {
  DCHECK(base::PlatformThread::CurrentId() == GetThreadId());

  TRACE_EVENT0("gpu", "GpuVSyncWorker::WaitForVSyncOnThread");

  HMONITOR monitor =
      MonitorFromWindow(surface_handle_, MONITOR_DEFAULTTONEAREST);
  MONITORINFOEX monitor_info;
  monitor_info.cbSize = sizeof(MONITORINFOEX);
  BOOL success = GetMonitorInfo(monitor, &monitor_info);
  CHECK(success);

  if (current_device_name_.compare(monitor_info.szDevice) != 0) {
    // Monitor changed. Close the current adapter handle and open a new one.
    CloseAdapter();
    OpenAdapter(monitor_info.szDevice);
  }

  if (WaitForVBlankEvent()) {
    // Note: this sends update on background thread which the callback is
    // expected to handle.
    SendVSyncUpdate(base::TimeTicks::Now());
  }

  Reschedule();
}

void GpuVSyncWorker::SendVSyncUpdate(base::TimeTicks timestamp) {
  if (base::subtle::NoBarrier_Load(&enabled_)) {
    TRACE_EVENT0("gpu", "GpuVSyncWorker::SendVSyncUpdate");
    callback_.Run(timestamp);
  }
}

void GpuVSyncWorker::Reschedule() {
  // Restart the task if still enabled.
  if (base::subtle::NoBarrier_Load(&enabled_)) {
    task_runner()->PostTask(FROM_HERE,
                            base::Bind(&GpuVSyncWorker::WaitForVSyncOnThread,
                                       base::Unretained(this)));
  } else {
    running_ = false;
  }
}

void GpuVSyncWorker::OpenAdapter(const wchar_t* device_name) {
  DCHECK_EQ(0u, current_adapter_handle_);

  HDC hdc = CreateDC(NULL, device_name, NULL, NULL);

  D3DKMT_OPENADAPTERFROMHDC open_adapter_data;
  open_adapter_data.hDc = hdc;

  NTSTATUS result = open_adapter_from_hdc_ptr_(&open_adapter_data);
  DeleteDC(hdc);

  CHECK(result == STATUS_SUCCESS);

  current_device_name_ = device_name;
  current_adapter_handle_ = open_adapter_data.hAdapter;
  current_source_id_ = open_adapter_data.VidPnSourceId;
}

void GpuVSyncWorker::CloseAdapter() {
  if (current_adapter_handle_ != 0) {
    D3DKMT_CLOSEADAPTER close_adapter_data;
    close_adapter_data.hAdapter = current_adapter_handle_;

    NTSTATUS result = close_adapter_ptr_(&close_adapter_data);
    CHECK(result == STATUS_SUCCESS);

    current_adapter_handle_ = 0;
    current_device_name_.clear();
  }
}

bool GpuVSyncWorker::WaitForVBlankEvent() {
  D3DKMT_WAITFORVERTICALBLANKEVENT wait_for_vertical_blank_event_data;
  wait_for_vertical_blank_event_data.hAdapter = current_adapter_handle_;
  wait_for_vertical_blank_event_data.hDevice = 0;
  wait_for_vertical_blank_event_data.VidPnSourceId = current_source_id_;

  NTSTATUS result =
      wait_for_vertical_blank_event_ptr_(&wait_for_vertical_blank_event_data);

  return result == STATUS_SUCCESS;
}

/* static */
std::unique_ptr<GpuVSyncProvider> GpuVSyncProvider::Create(
    const VSyncCallback& callback,
    SurfaceHandle surface_handle) {
  return std::unique_ptr<GpuVSyncProvider>(
      new GpuVSyncProvider(callback, surface_handle));
}

GpuVSyncProvider::GpuVSyncProvider(const VSyncCallback& callback,
                                   SurfaceHandle surface_handle)
    : vsync_worker_(new GpuVSyncWorker(callback, surface_handle)) {
  // Start the thread.
  base::Thread::Options options;
  // TODO(stanisc): might consider even higher priority - REALTIME_AUDIO.
  options.priority = base::ThreadPriority::DISPLAY;
  vsync_worker_->StartWithOptions(options);
}

GpuVSyncProvider::~GpuVSyncProvider() = default;

void GpuVSyncProvider::EnableVSync(bool enabled) {
  vsync_worker_->Enable(enabled);
}

}  // namespace gpu
