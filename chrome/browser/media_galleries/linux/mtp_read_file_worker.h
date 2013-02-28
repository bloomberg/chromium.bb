// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_READ_FILE_WORKER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_READ_FILE_WORKER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/cancellation_flag.h"
#include "chrome/browser/media_galleries/linux/mtp_device_operations_utils.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}

namespace chrome {

typedef struct WorkerDeleter<class MTPReadFileWorker> MTPReadFileWorkerDeleter;

// Worker class to read media device file data given a file |path|.
class MTPReadFileWorker
    : public base::RefCountedThreadSafe<MTPReadFileWorker,
                                        MTPReadFileWorkerDeleter> {
 public:
  // Constructed on |media_task_runner_| thread.
  MTPReadFileWorker(const std::string& handle,
                    const std::string& src_path,
                    uint32 total_size,
                    const base::FilePath& dest_path,
                    base::SequencedTaskRunner* task_runner,
                    base::WaitableEvent* task_completed_event,
                    base::WaitableEvent* shutdown_event);

  // This function is invoked on |media_task_runner_| to post the task on UI
  // thread. This blocks the |media_task_runner_| until the task is complete.
  void Run();

  bool Succeeded() const;

  // Returns the |media_task_runner_| associated with this worker object.
  // This function is exposed for WorkerDeleter struct to access the
  // |media_task_runner_|.
  base::SequencedTaskRunner* media_task_runner() const {
    return media_task_runner_.get();
  }

 private:
  friend struct WorkerDeleter<MTPReadFileWorker>;
  friend class base::DeleteHelper<MTPReadFileWorker>;
  friend class base::RefCountedThreadSafe<MTPReadFileWorker,
                                          MTPReadFileWorkerDeleter>;

  // Destructed via MTPReadFileWorkerDeleter.
  virtual ~MTPReadFileWorker();

  // Dispatches a request to MediaTransferProtocolManager to get the media file
  // contents.
  void DoWorkOnUIThread();

  // Query callback for DoWorkOnUIThread(). On success, |data| has the media
  // file contents. On failure, |error| is set to true. This function signals
  // to unblock |media_task_runner_|.
  void OnDidWorkOnUIThread(const std::string& data, bool error);

  uint32 BytesToRead() const;

  // The device unique identifier to query the device.
  const std::string device_handle_;

  // The media device file path.
  const std::string src_path_;

  // Number of bytes to read.
  const uint32 total_bytes_;

  // Where to write the data read from the device.
  const base::FilePath dest_path_;

  /****************************************************************************
   * The variables below are accessed on both |media_task_runner_| and the UI
   * thread. However, there's no concurrent access because the UI thread is in a
   * blocked state when access occurs on |media_task_runner_|.
   */

  // Number of bytes read from the device.
  uint32 bytes_read_;

  // Temporary data storage.
  std::string data_;

  // Whether an error occurred during file transfer.
  bool error_occurred_;

  /***************************************************************************/

  // A reference to |media_task_runner_| to destruct this object on the correct
  // thread.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // |media_task_runner_| can wait on this event until the required operation
  // is complete.
  // TODO(kmadhusu): Remove this WaitableEvent after modifying the
  // DeviceMediaFileUtil functions as asynchronous functions.
  base::WaitableEvent* on_task_completed_event_;

  // Stores a reference to waitable event associated with the shut down message.
  base::WaitableEvent* on_shutdown_event_;

  // Set to ignore the request results. This will be set when
  // MTPDeviceDelegateImplLinux object is about to be deleted.
  // |on_task_completed_event_| and |on_shutdown_event_| should not be
  // dereferenced when this is set.
  base::CancellationFlag cancel_tasks_flag_;

  DISALLOW_COPY_AND_ASSIGN(MTPReadFileWorker);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_READ_FILE_WORKER_H_
