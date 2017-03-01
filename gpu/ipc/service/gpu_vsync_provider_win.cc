// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_vsync_provider_win.h"

#include <string>

#include "base/atomicops.h"
#include "base/debug/alias.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/message_filter.h"
#include "ui/gl/vsync_provider_win.h"

#include <windows.h>

namespace gpu {

namespace {
// Default interval used when no v-sync interval comes from DWM.
const int kDefaultTimerBasedInterval = 16666;

// from <D3dkmthk.h>
typedef LONG NTSTATUS;
typedef UINT D3DKMT_HANDLE;
typedef UINT D3DDDI_VIDEO_PRESENT_SOURCE_ID;

#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_GRAPHICS_PRESENT_OCCLUDED ((NTSTATUS)0xC01E0006L)

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
class GpuVSyncWorker : public base::Thread,
                       public base::RefCountedThreadSafe<GpuVSyncWorker> {
 public:
  GpuVSyncWorker(const gfx::VSyncProvider::UpdateVSyncCallback& callback,
                 SurfaceHandle surface_handle);

  void CleanupAndStop();
  void Enable(bool enabled);
  void StartRunningVSyncOnThread();
  void WaitForVSyncOnThread();
  bool BelongsToWorkerThread();

 private:
  friend class base::RefCountedThreadSafe<GpuVSyncWorker>;
  ~GpuVSyncWorker() override;

  void Reschedule();
  void OpenAdapter(const wchar_t* device_name);
  void CloseAdapter();
  NTSTATUS WaitForVBlankEvent();

  void SendGpuVSyncUpdate(base::TimeTicks now,
                          base::TimeTicks timestamp,
                          base::TimeDelta interval);
  void InvokeCallbackAndReschedule(base::TimeTicks timestamp,
                                   base::TimeDelta interval);
  void ScheduleDelayBasedVSync(base::TimeTicks timebase,
                               base::TimeDelta interval);

  // Specifies whether background tasks are running.
  // This can be set on background thread only.
  bool running_ = false;

  // Specified whether the worker is enabled.  This is accessed from both
  // threads but can be changed on the main thread only.
  base::subtle::AtomicWord enabled_ = false;

  const gfx::VSyncProvider::UpdateVSyncCallback callback_;
  const SurfaceHandle surface_handle_;

  // The actual timing and interval comes from the nested provider.
  std::unique_ptr<gl::VSyncProviderWin> vsync_provider_;

  PFND3DKMTOPENADAPTERFROMHDC open_adapter_from_hdc_ptr_;
  PFND3DKMTCLOSEADAPTER close_adapter_ptr_;
  PFND3DKMTWAITFORVERTICALBLANKEVENT wait_for_vertical_blank_event_ptr_;

  std::wstring current_device_name_;
  D3DKMT_HANDLE current_adapter_handle_ = 0;
  D3DDDI_VIDEO_PRESENT_SOURCE_ID current_source_id_ = 0;
};

GpuVSyncWorker::GpuVSyncWorker(
    const gfx::VSyncProvider::UpdateVSyncCallback& callback,
    SurfaceHandle surface_handle)
    : base::Thread(base::StringPrintf("VSync-%d", surface_handle)),
      callback_(callback),
      surface_handle_(surface_handle),
      vsync_provider_(new gl::VSyncProviderWin(surface_handle)) {
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

GpuVSyncWorker::~GpuVSyncWorker() = default;

void GpuVSyncWorker::CleanupAndStop() {
  Enable(false);
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

bool GpuVSyncWorker::BelongsToWorkerThread() {
  return base::PlatformThread::CurrentId() == GetThreadId();
}

void GpuVSyncWorker::StartRunningVSyncOnThread() {
  DCHECK(BelongsToWorkerThread());

  if (!running_) {
    running_ = true;
    WaitForVSyncOnThread();
  }
}

void GpuVSyncWorker::WaitForVSyncOnThread() {
  DCHECK(BelongsToWorkerThread());

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

  NTSTATUS wait_result = WaitForVBlankEvent();
  if (wait_result != STATUS_SUCCESS) {
    if (wait_result == STATUS_GRAPHICS_PRESENT_OCCLUDED) {
      // This may be triggered by the monitor going into sleep.
      // Use timer based mechanism as a backup, start with getting VSync
      // parameters to determine timebase and interval.
      // TODO(stanisc): Consider a slower v-sync rate in this particular case.
      vsync_provider_->GetVSyncParameters(base::Bind(
          &GpuVSyncWorker::ScheduleDelayBasedVSync, base::Unretained(this)));
      return;
    } else {
      base::debug::Alias(&wait_result);
      CHECK(false);
    }
  }

  vsync_provider_->GetVSyncParameters(
      base::Bind(&GpuVSyncWorker::SendGpuVSyncUpdate, base::Unretained(this),
                 base::TimeTicks::Now()));
}

void GpuVSyncWorker::SendGpuVSyncUpdate(base::TimeTicks now,
                                        base::TimeTicks timestamp,
                                        base::TimeDelta interval) {
  base::TimeDelta adjustment;

  if (!(timestamp.is_null() || interval.is_zero())) {
    // Timestamp comes from DwmGetCompositionTimingInfo and apparently it might
    // be up to 2-3 vsync cycles in the past or in the future.
    // The adjustment formula was suggested here:
    // http://www.vsynctester.com/firefoxisbroken.html
    adjustment =
        ((now - timestamp + interval / 8) % interval + interval) % interval -
        interval / 8;
    timestamp = now - adjustment;
  } else {
    // DWM must be disabled.
    timestamp = now;
  }

  TRACE_EVENT1("gpu", "GpuVSyncWorker::SendGpuVSyncUpdate", "adjustment",
               adjustment.ToInternalValue());

  InvokeCallbackAndReschedule(timestamp, interval);
}

void GpuVSyncWorker::InvokeCallbackAndReschedule(base::TimeTicks timestamp,
                                                 base::TimeDelta interval) {
  // Send update and restart the task if still enabled.
  if (base::subtle::NoBarrier_Load(&enabled_)) {
    callback_.Run(timestamp, interval);
    task_runner()->PostTask(FROM_HERE,
                            base::Bind(&GpuVSyncWorker::WaitForVSyncOnThread,
                                       base::Unretained(this)));
  } else {
    running_ = false;
  }
}

void GpuVSyncWorker::ScheduleDelayBasedVSync(base::TimeTicks timebase,
                                             base::TimeDelta interval) {
  // This is called only when WaitForVBlankEvent fails due to monitor going to
  // sleep.  Use a delay based v-sync as a back-up.
  if (interval.is_zero()) {
    interval = base::TimeDelta::FromMicroseconds(kDefaultTimerBasedInterval);
  }

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks next_vsync = now.SnappedToNextTick(timebase, interval);

  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GpuVSyncWorker::InvokeCallbackAndReschedule,
                 base::Unretained(this), next_vsync, interval),
      next_vsync - now);
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

NTSTATUS GpuVSyncWorker::WaitForVBlankEvent() {
  D3DKMT_WAITFORVERTICALBLANKEVENT wait_for_vertical_blank_event_data;
  wait_for_vertical_blank_event_data.hAdapter = current_adapter_handle_;
  wait_for_vertical_blank_event_data.hDevice = 0;
  wait_for_vertical_blank_event_data.VidPnSourceId = current_source_id_;

  return wait_for_vertical_blank_event_ptr_(
      &wait_for_vertical_blank_event_data);
}

// MessageFilter class for sending and receiving IPC messages
// directly, avoiding routing them through the main GPU thread.
class GpuVSyncMessageFilter : public IPC::MessageFilter {
 public:
  explicit GpuVSyncMessageFilter(
      const scoped_refptr<GpuVSyncWorker>& vsync_worker,
      int32_t route_id)
      : vsync_worker_(vsync_worker), route_id_(route_id) {}

  // IPC::MessageFilter overrides.
  void OnChannelError() override { Reset(); }
  void OnChannelClosing() override { Reset(); }
  void OnFilterAdded(IPC::Channel* channel) override;
  void OnFilterRemoved() override { Reset(); }
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Send can be called from GpuVSyncWorker thread.
  void Send(std::unique_ptr<IPC::Message> message);

  int32_t route_id() const { return route_id_; }

 private:
  ~GpuVSyncMessageFilter() override = default;
  void SendOnIOThread(std::unique_ptr<IPC::Message> message);
  void Reset();

  scoped_refptr<GpuVSyncWorker> vsync_worker_;
  // The sender to which this filter was added.
  IPC::Sender* sender_ = nullptr;
  // The sender must be invoked on IO thread.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  const int32_t route_id_;
};

void GpuVSyncMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  sender_ = channel;
}

void GpuVSyncMessageFilter::Reset() {
  sender_ = nullptr;
  vsync_worker_->Enable(false);
}

bool GpuVSyncMessageFilter::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() != route_id_)
    return false;

  IPC_BEGIN_MESSAGE_MAP(GpuVSyncMessageFilter, msg)
    IPC_MESSAGE_FORWARD(GpuCommandBufferMsg_SetNeedsVSync, vsync_worker_.get(),
                        GpuVSyncWorker::Enable);
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()
  return true;
}

void GpuVSyncMessageFilter::Send(std::unique_ptr<IPC::Message> message) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuVSyncMessageFilter::SendOnIOThread, this,
                            base::Passed(&message)));
}

void GpuVSyncMessageFilter::SendOnIOThread(
    std::unique_ptr<IPC::Message> message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(!message->is_sync());
  if (!sender_)
    return;

  sender_->Send(message.release());
}

GpuVSyncProviderWin::GpuVSyncProviderWin(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle) {
  vsync_worker_ = new GpuVSyncWorker(
      base::Bind(&GpuVSyncProviderWin::OnVSync, base::Unretained(this)),
      surface_handle);
  message_filter_ =
      new GpuVSyncMessageFilter(vsync_worker_, delegate->GetRouteID());
  delegate->AddFilter(message_filter_.get());

  // Start the thread.
  base::Thread::Options options;
  // TODO(stanisc): might consider even higher priority - REALTIME_AUDIO.
  options.priority = base::ThreadPriority::DISPLAY;
  vsync_worker_->StartWithOptions(options);
}

GpuVSyncProviderWin::~GpuVSyncProviderWin() {
  vsync_worker_->CleanupAndStop();
}

void GpuVSyncProviderWin::GetVSyncParameters(
    const UpdateVSyncCallback& callback) {
  // This is ignored and the |callback| is never called back.  The timestamp
  // and interval are posted directly via
  // GpuCommandBufferMsg_UpdateVSyncParameters message sent from the worker
  // thread.
}

void GpuVSyncProviderWin::OnVSync(base::TimeTicks timestamp,
                                  base::TimeDelta interval) {
  DCHECK(vsync_worker_->BelongsToWorkerThread());

  message_filter_->Send(
      base::MakeUnique<GpuCommandBufferMsg_UpdateVSyncParameters>(
          message_filter_->route_id(), timestamp, interval));
}

}  // namespace gpu
