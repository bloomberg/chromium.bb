// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_GET_FILE_INFO_WORKER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_GET_FILE_INFO_WORKER_H_

#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/cancellation_flag.h"
#include "chrome/browser/media_galleries/linux/mtp_device_operations_utils.h"
#include "device/media_transfer_protocol/mtp_file_entry.pb.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}

namespace chrome {

class MTPGetFileInfoWorker;
typedef struct WorkerDeleter<class MTPGetFileInfoWorker>
    MTPGetFileInfoWorkerDeleter;

// Worker class to get media device file information given a |path|.
class MTPGetFileInfoWorker
    : public base::RefCountedThreadSafe<MTPGetFileInfoWorker,
                                        MTPGetFileInfoWorkerDeleter> {
 public:
  // Constructed on |media_task_runner_| thread.
  MTPGetFileInfoWorker(const std::string& handle,
                       const std::string& path,
                       base::SequencedTaskRunner* task_runner,
                       base::WaitableEvent* task_completed_event,
                       base::WaitableEvent* shutdown_event);

  // This function is invoked on |media_task_runner_| to post the task on UI
  // thread. This blocks the |media_task_runner_| until the task is complete.
  void Run();

  // Returns GetFileInfo() result and fills in |file_info| with requested file
  // entry details.
  base::PlatformFileError get_file_info(
      base::PlatformFileInfo* file_info) const;

  // Returns the |media_task_runner_| associated with this worker object.
  // This function is exposed for WorkerDeleter struct to access the
  // |media_task_runner_|.
  base::SequencedTaskRunner* media_task_runner() const {
    return media_task_runner_.get();
  }

 private:
  friend struct WorkerDeleter<MTPGetFileInfoWorker>;
  friend class base::DeleteHelper<MTPGetFileInfoWorker>;
  friend class base::RefCountedThreadSafe<MTPGetFileInfoWorker,
                                          MTPGetFileInfoWorkerDeleter>;

  // Destructed via MTPGetFileInfoWorkerDeleter.
  virtual ~MTPGetFileInfoWorker();

  // Dispatches a request to MediaTransferProtocolManager to get file
  // information.
  void DoWorkOnUIThread();

  // Query callback for DoWorkOnUIThread(). On success, |file_entry| has media
  // file information. On failure, |error| is set to true. This function signals
  // to unblock |media_task_runner_|.
  void OnDidWorkOnUIThread(const MtpFileEntry& file_entry, bool error);

  // Stores the device handle to query the device.
  const std::string device_handle_;

  // Stores the requested media device file path.
  const std::string path_;

  // Stores a reference to |media_task_runner_| to destruct this object on the
  // correct thread.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // Stores the result of GetFileInfo(). Normally accessed on
  // |media_task_runner_| but it is also safe to access on the UI thread when
  // |media_task_runner_| is blocked.
  base::PlatformFileError error_;

  // Stores the media file entry information. Same access policy as |error_|.
  base::PlatformFileInfo file_entry_info_;

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

  DISALLOW_COPY_AND_ASSIGN(MTPGetFileInfoWorker);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_GET_FILE_INFO_WORKER_H_
