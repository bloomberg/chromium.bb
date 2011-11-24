// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_PROVIDER_H_
#define CONTENT_BROWSER_GAMEPAD_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/shared_memory.h"
#include "base/system_monitor/system_monitor.h"
#include "base/task.h"
#include "content/browser/gamepad/data_fetcher.h"
#include "content/common/content_export.h"
#include "content/common/gamepad_hardware_buffer.h"

namespace base {
class Thread;
}

struct GamepadMsg_Updated_Params;

namespace gamepad {

class CONTENT_EXPORT Provider :
    public base::RefCountedThreadSafe<Provider>,
    public base::SystemMonitor::DevicesChangedObserver {
 public:
  explicit Provider(DataFetcher* fetcher);

  // Starts or Stops the provider. Called from creator_loop_.
  void Start();
  void Stop();
  base::SharedMemoryHandle GetRendererSharedMemoryHandle(
      base::ProcessHandle renderer_process);

 private:
  friend class base::RefCountedThreadSafe<Provider>;

  virtual ~Provider();

  // Method for starting the polling, runs on polling_thread_.
  void DoInitializePollingThread();

  // Method for polling a DataFetcher. Runs on the polling_thread_.
  void DoPoll();
  void ScheduleDoPoll();

  virtual void OnDevicesChanged() OVERRIDE;

  GamepadHardwareBuffer* SharedMemoryAsHardwareBuffer();

  enum { kDesiredSamplingIntervalMs = 16 };

  // The Message Loop on which this object was created.
  // Typically the I/O loop, but may be something else during testing.
  scoped_refptr<base::MessageLoopProxy> creator_loop_;
  scoped_ptr<DataFetcher> provided_fetcher_;

  // When polling_thread_ is running, members below are only to be used
  // from that thread.
  scoped_ptr<DataFetcher> data_fetcher_;
  base::SharedMemory gamepad_shared_memory_;
  bool devices_changed_;

  // Polling is done on this background thread.
  scoped_ptr<base::Thread> polling_thread_;

  static Provider* instance_;
  base::WeakPtrFactory<Provider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Provider);
};

}  // namespace gamepad

#endif  // CONTENT_BROWSER_GAMEPAD_PROVIDER_H_
