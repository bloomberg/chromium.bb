// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class opens the media transfer protocol (MTP) storage device for
// communication. This class is created, destroyed and operated on the blocking
// thread. It may take a while to open the device for communication.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_OPEN_STORAGE_WORKER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_OPEN_STORAGE_WORKER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/cancellation_flag.h"
#include "chrome/browser/media_galleries/linux/mtp_device_operations_utils.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}

namespace chrome {

typedef struct WorkerDeleter<class MTPOpenStorageWorker>
    MTPOpenStorageWorkerDeleter;

// Worker class to open a MTP device for communication. This class is
// instantiated and destructed on |media_task_runner_|. In order to post a
// request on Dbus thread, the caller should run on UI thread. Therefore, this
// class posts the open device request on UI thread and receives the response
// on UI thread.
class MTPOpenStorageWorker
    : public base::RefCountedThreadSafe<MTPOpenStorageWorker,
                                        MTPOpenStorageWorkerDeleter> {
 public:
  // Constructed on |media_task_runner_| thread.
  MTPOpenStorageWorker(const std::string& name,
                       base::SequencedTaskRunner* task_runner,
                       base::WaitableEvent* task_completed_event,
                       base::WaitableEvent* shutdown_event);

  // This function is invoked on |media_task_runner_| to post the task on UI
  // thread. This blocks the |media_task_runner_| until the task is complete.
  void Run();

  // Returns a device handle string if the open storage request was
  // successfully completed or an empty string otherwise.
  const std::string& device_handle() const { return device_handle_; }

  // Returns the |media_task_runner_| associated with this worker object.
  // This function is exposed for WorkerDeleter struct to access the
  // |media_task_runner_|.
  base::SequencedTaskRunner* media_task_runner() const {
    return media_task_runner_.get();
  }

 private:
  friend struct WorkerDeleter<MTPOpenStorageWorker>;
  friend class base::DeleteHelper<MTPOpenStorageWorker>;
  friend class base::RefCountedThreadSafe<MTPOpenStorageWorker,
                                          MTPOpenStorageWorkerDeleter>;

  // Destructed via MTPOpenStorageWorkerDeleter struct.
  virtual ~MTPOpenStorageWorker();

  // Dispatches a request to MediaTransferProtocolManager to open the MTP
  // storage for communication. This is called on UI thread.
  void DoWorkOnUIThread();

  // Query callback for DoWorkOnUIThread(). |error| is set to true if the device
  // did not open successfully. This function signals to unblock
  // |media_task_runner_|.
  void OnDidWorkOnUIThread(const std::string& device_handle, bool error);

  // The MTP device storage name.
  const std::string storage_name_;

  // Stores a reference to |media_task_runner_| to destruct this object on the
  // correct thread.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // |media_task_runner_| can wait on this event until the required operation
  // is complete.
  // TODO(kmadhusu): Remove this WaitableEvent after modifying the
  // DeviceMediaFileUtil functions as asynchronous functions.
  base::WaitableEvent* on_task_completed_event_;

  // Stores a reference to waitable event associated with the shut down message.
  base::WaitableEvent* on_shutdown_event_;

  // Stores the result of an open storage request. Read from
  // |media_task_runner_| and written to on the UI thread only when
  // |media_task_runner_| is blocked.
  std::string device_handle_;

  // Set to ignore the request results. This will be set when
  // MTPDeviceDelegateImplLinux object is about to be deleted.
  // |on_task_completed_event_| and |on_shutdown_event_| should not be
  // dereferenced when this is set.
  base::CancellationFlag cancel_tasks_flag_;

  DISALLOW_COPY_AND_ASSIGN(MTPOpenStorageWorker);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_OPEN_STORAGE_WORKER_H_
