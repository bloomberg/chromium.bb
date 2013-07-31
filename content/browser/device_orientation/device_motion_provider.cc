// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/device_motion_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "content/browser/device_orientation/data_fetcher_shared_memory.h"
#include "content/common/device_motion_hardware_buffer.h"

namespace content {

namespace {
const int kPeriodInMilliseconds = 100;
}

class DeviceMotionProvider::PollingThread : public base::Thread {
 public:
  explicit PollingThread(const char* name);
  virtual ~PollingThread();

  void StartPolling(DataFetcherSharedMemory* fetcher,
                    DeviceMotionHardwareBuffer* buffer);
  void StopPolling();

 private:
  void DoPoll();

  scoped_ptr<base::RepeatingTimer<PollingThread> > timer_;
  DataFetcherSharedMemory* fetcher_;

  DISALLOW_COPY_AND_ASSIGN(PollingThread);
};

// ---- PollingThread methods

DeviceMotionProvider::PollingThread::PollingThread(const char* name)
    : base::Thread(name) {
}

DeviceMotionProvider::PollingThread::~PollingThread() {
}

void DeviceMotionProvider::PollingThread::StartPolling(
    DataFetcherSharedMemory* fetcher, DeviceMotionHardwareBuffer* buffer) {
  DCHECK(base::MessageLoop::current() == message_loop());
  DCHECK(!timer_);

  fetcher_ = fetcher;
  fetcher_->StartFetchingDeviceMotionData(buffer);
  timer_.reset(new base::RepeatingTimer<PollingThread>());
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromMilliseconds(kPeriodInMilliseconds),
                this, &PollingThread::DoPoll);
}

void DeviceMotionProvider::PollingThread::StopPolling() {
  DCHECK(base::MessageLoop::current() == message_loop());
  DCHECK(fetcher_);
  // this will also stop the timer before killing it.
  timer_.reset();
  fetcher_->StopFetchingDeviceMotionData();
}

void DeviceMotionProvider::PollingThread::DoPoll() {
  DCHECK(base::MessageLoop::current() == message_loop());
  fetcher_->FetchDeviceMotionDataIntoBuffer();
}

// ---- end PollingThread methods

DeviceMotionProvider::DeviceMotionProvider()
    : is_started_(false) {
  Initialize();
}

DeviceMotionProvider::DeviceMotionProvider(
    scoped_ptr<DataFetcherSharedMemory> fetcher)
    : is_started_(false) {
  data_fetcher_ = fetcher.Pass();
  Initialize();
}

DeviceMotionProvider::~DeviceMotionProvider() {
  StopFetchingDeviceMotionData();
  // make sure polling thread stops before data_fetcher_ gets deleted.
  if (polling_thread_)
    polling_thread_->Stop();
  data_fetcher_.reset();
}

void DeviceMotionProvider::Initialize() {
  size_t data_size = sizeof(DeviceMotionHardwareBuffer);
  bool res = device_motion_shared_memory_.CreateAndMapAnonymous(data_size);
  // TODO(timvolodine): consider not crashing the browser if the check fails.
  CHECK(res);
  DeviceMotionHardwareBuffer* hwbuf = SharedMemoryAsHardwareBuffer();
  memset(hwbuf, 0, sizeof(DeviceMotionHardwareBuffer));
}

base::SharedMemoryHandle DeviceMotionProvider::GetSharedMemoryHandleForProcess(
    base::ProcessHandle process) {
  base::SharedMemoryHandle renderer_handle;
  device_motion_shared_memory_.ShareToProcess(process, &renderer_handle);
  return renderer_handle;
}

void DeviceMotionProvider::StartFetchingDeviceMotionData() {
  if (is_started_)
    return;

  if (!data_fetcher_)
    data_fetcher_.reset(new DataFetcherSharedMemory);

  if (data_fetcher_->NeedsPolling()) {
    if (!polling_thread_)
        CreateAndStartPollingThread();

    polling_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&PollingThread::StartPolling,
                   base::Unretained(polling_thread_.get()),
                   data_fetcher_.get(),
                   SharedMemoryAsHardwareBuffer()));
  } else {
    data_fetcher_->StartFetchingDeviceMotionData(
        SharedMemoryAsHardwareBuffer());
  }

  is_started_ = true;
}

void DeviceMotionProvider::CreateAndStartPollingThread() {
  polling_thread_.reset(
      new PollingThread("Device Motion poller"));

  if (!polling_thread_->Start()) {
      LOG(ERROR) << "Failed to start Device Motion data polling thread";
      return;
  }
}

void DeviceMotionProvider::StopFetchingDeviceMotionData() {
  if (!is_started_)
    return;

  DCHECK(data_fetcher_);

  if (data_fetcher_->NeedsPolling()) {
    DCHECK(polling_thread_);
    polling_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&PollingThread::StopPolling,
                   base::Unretained(polling_thread_.get())));
  } else {
    data_fetcher_->StopFetchingDeviceMotionData();
  }

  is_started_ = false;
}

DeviceMotionHardwareBuffer*
DeviceMotionProvider::SharedMemoryAsHardwareBuffer() {
  void* mem = device_motion_shared_memory_.memory();
  CHECK(mem);
  return static_cast<DeviceMotionHardwareBuffer*>(mem);
}

}  // namespace content
