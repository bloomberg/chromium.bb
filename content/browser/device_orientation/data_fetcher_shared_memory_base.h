// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_SHARED_MEMORY_BASE_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_SHARED_MEMORY_BASE_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/device_orientation/inertial_sensor_consts.h"
#include "content/common/content_export.h"

namespace content {

// Sensor data fetchers should derive from this base class and implement
// the abstract Start() and Stop() methods.
// If the fetcher requires polling it should also implement IsPolling()
// to return true and the Fetch() method which will be called from the
// polling thread to fetch data at regular intervals.
class CONTENT_EXPORT DataFetcherSharedMemoryBase {
 public:

  // Starts updating the shared memory buffer with sensor data at
  // regular intervals. Returns true if the relevant sensors could
  // be successfully activated.
  bool StartFetchingDeviceData(ConsumerType consumer_type);

  // Stops updating the shared memory buffer. Returns true if the
  // relevant sensors could be successfully deactivated.
  bool StopFetchingDeviceData(ConsumerType consumer_type);

  // Returns the shared memory handle of the device sensor data
  // duplicated into the given process. This method should only be
  // called after a call to StartFetchingDeviceData method with
  // corresponding |consumer_type| parameter.
  base::SharedMemoryHandle GetSharedMemoryHandleForProcess(
      ConsumerType consumer_type, base::ProcessHandle process);

 protected:
  class PollingThread;

  DataFetcherSharedMemoryBase();
  virtual ~DataFetcherSharedMemoryBase();

  // Returns the message loop of the polling thread.
  // Returns NULL if there is no polling thread.
  base::MessageLoop* GetPollingMessageLoop() const;

  // If IsPolling() is true this method is called from the |polling_thread_|
  // at regular intervals.
  virtual void Fetch(unsigned consumer_bitmask);

  // Returns true if this fetcher needs to use a polling thread to
  // fetch the sensor data.
  virtual bool IsPolling() const;

  // Start() method should call InitSharedMemoryBuffer() to get the shared
  // memory pointer. If IsPolling() is true both Start() and Stop() methods
  // are called from the |polling_thread_|.
  virtual bool Start(ConsumerType consumer_type, void* buffer) = 0;
  virtual bool Stop(ConsumerType consumer_type) = 0;

 private:
  bool InitAndStartPollingThreadIfNecessary();
  base::SharedMemory* GetSharedMemory(ConsumerType consumer_type);
  void* GetSharedMemoryBuffer(ConsumerType consumer_type);

  unsigned started_consumers_;

  scoped_ptr<PollingThread> polling_thread_;

  // Owning pointers. Objects in the map are deleted in dtor.
  typedef std::map<ConsumerType, base::SharedMemory*> SharedMemoryMap;
  SharedMemoryMap shared_memory_map_;

  DISALLOW_COPY_AND_ASSIGN(DataFetcherSharedMemoryBase);
};

}

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_SHARED_MEMORY_BASE_H_
