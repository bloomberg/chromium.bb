// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_PROVIDER_H_
#define CONTENT_BROWSER_GAMEPAD_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/shared_memory.h"
#include "base/synchronization/lock.h"
#include "base/system_monitor/system_monitor.h"
#include "base/task.h"
#include "content/browser/gamepad/data_fetcher.h"
#include "content/common/content_export.h"
#include "content/common/gamepad_hardware_buffer.h"

namespace base {
class Thread;
}

struct GamepadMsg_Updated_Params;

namespace content {

class CONTENT_EXPORT GamepadProvider :
    public base::RefCountedThreadSafe<GamepadProvider>,
    public base::SystemMonitor::DevicesChangedObserver {
 public:
  explicit GamepadProvider(GamepadDataFetcher* fetcher);

  base::SharedMemoryHandle GetRendererSharedMemoryHandle(
      base::ProcessHandle renderer_process);

  // Pause and resume the background polling thread. Can be called from any
  // thread.
  void Pause();
  void Resume();

 private:
  friend class base::RefCountedThreadSafe<GamepadProvider>;

  virtual ~GamepadProvider();

  // Method for starting the polling, runs on polling_thread_.
  void DoInitializePollingThread();

  // Method for polling a GamepadDataFetcher. Runs on the polling_thread_.
  void DoPoll();
  void ScheduleDoPoll();

  virtual void OnDevicesChanged() OVERRIDE;

  GamepadHardwareBuffer* SharedMemoryAsHardwareBuffer();

  enum { kDesiredSamplingIntervalMs = 16 };

  // Keeps track of when the background thread is paused. Access to is_paused_
  // must be guarded by is_paused_lock_.
  base::Lock is_paused_lock_;
  bool is_paused_;

  // Updated based on notification from SystemMonitor when the system devices
  // have been updated, and this notification is passed on to the data fetcher
  // to enable it to avoid redundant (and possibly expensive) is-connected
  // tests. Access to devices_changed_ must be guarded by
  // devices_changed_lock_.
  base::Lock devices_changed_lock_;
  bool devices_changed_;

  // The Message Loop on which this object was created.
  // Typically the I/O loop, but may be something else during testing.
  scoped_ptr<GamepadDataFetcher> provided_fetcher_;

  // When polling_thread_ is running, members below are only to be used
  // from that thread.
  scoped_ptr<GamepadDataFetcher> data_fetcher_;
  base::SharedMemory gamepad_shared_memory_;

  // Polling is done on this background thread.
  scoped_ptr<base::Thread> polling_thread_;

  static GamepadProvider* instance_;
  base::WeakPtrFactory<GamepadProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GamepadProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_PROVIDER_H_
