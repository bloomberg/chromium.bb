// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_READ_DIRECTORY_WORKER_H_
#define CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_READ_DIRECTORY_WORKER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/cancellation_flag.h"
#include "chrome/browser/media_gallery/linux/mtp_device_operations_utils.h"
#include "chrome/browser/media_transfer_protocol/mtp_file_entry.pb.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}

namespace chrome {

class MTPReadDirectoryWorker;
typedef struct WorkerDeleter<class MTPReadDirectoryWorker>
    MTPReadDirectoryWorkerDeleter;

// Worker class to read directory contents. Device is already opened for
// communication.
class MTPReadDirectoryWorker
    : public base::RefCountedThreadSafe<MTPReadDirectoryWorker,
                                        MTPReadDirectoryWorkerDeleter> {
 public:
  // Construct a worker object given the directory |path|. This object is
  // constructed on |media_task_runner_| thread.
  MTPReadDirectoryWorker(const std::string& handle,
                         const std::string& path,
                         base::SequencedTaskRunner* task_runner,
                         base::WaitableEvent* task_completed_event,
                         base::WaitableEvent* shutdown_event);

  // Construct a worker object given the directory |entry_id|. This object is
  // constructed on |media_task_runner_| thread.
  MTPReadDirectoryWorker(const std::string& storage_name,
                         const uint32_t entry_id,
                         base::SequencedTaskRunner* task_runner,
                         base::WaitableEvent* task_completed_event,
                         base::WaitableEvent* shutdown_event);

  // This function is invoked on |media_task_runner_| to post the task on UI
  // thread. This blocks the |media_task_runner_| until the task is complete.
  void Run();

  // Returns the directory entries for the given directory path.
  const std::vector<MtpFileEntry>& get_file_entries() const {
    return file_entries_;
  }

  // Returns the |media_task_runner_| associated with this worker object.
  // This function is exposed for WorkerDeleter struct to access the
  // |media_task_runner_|.
  base::SequencedTaskRunner* media_task_runner() const {
    return media_task_runner_.get();
  }

 private:
  friend struct WorkerDeleter<MTPReadDirectoryWorker>;
  friend class base::DeleteHelper<MTPReadDirectoryWorker>;
  friend class base::RefCountedThreadSafe<MTPReadDirectoryWorker,
                                          MTPReadDirectoryWorkerDeleter>;

  // Destructed via MTPReadDirectoryWorkerDeleter.
  virtual ~MTPReadDirectoryWorker();

  // Dispatches a request to MediaTransferProtocolManager to read the directory
  // entries. This is called on UI thread.
  void DoWorkOnUIThread();

  // Query callback for DoWorkOnUIThread(). On success, |file_entries| has the
  // directory file entries. |error| is true if there was an error. This
  // function signals to unblock |media_task_runner_|.
  void OnDidWorkOnUIThread(const std::vector<MtpFileEntry>& file_entries,
                           bool error);

  // Stores the device handle to communicate with storage device.
  const std::string device_handle_;

  // Stores the directory path whose contents needs to be listed.
  const std::string dir_path_;

  // Stores the directory entry id whose contents needs to be listed.
  const uint32_t dir_entry_id_;

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

  // Stores the result of a read directory request. Read from
  // |media_task_runner_| and written to on the UI thread only when
  // |media_task_runner_| is blocked.
  std::vector<MtpFileEntry> file_entries_;

  // Set to ignore the request results. This will be set when
  // MTPDeviceDelegateImplLinux object is about to be deleted.
  // |on_task_completed_event_| and |on_shutdown_event_| should not be
  // dereferenced when this is set.
  base::CancellationFlag cancel_tasks_flag_;

  DISALLOW_COPY_AND_ASSIGN(MTPReadDirectoryWorker);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_READ_DIRECTORY_WORKER_H_
