// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_CHANGE_PROCESSOR_ON_WORKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_CHANGE_PROCESSOR_ON_WORKER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
}

namespace sync_file_system {
namespace drive_backend {

class RemoteChangeProcessorWrapper;

// This class wraps a part of RemoteChangeProcessor class to post actual
// tasks to RemoteChangeProcessorWrapper which lives in another thread.
// Each method wraps corresponding name method of RemoteChangeProcessor.
// See comments in remote_change_processor.h for details.
class RemoteChangeProcessorOnWorker : public RemoteChangeProcessor {
 public:
  RemoteChangeProcessorOnWorker(
      const base::WeakPtr<RemoteChangeProcessorWrapper>& wrapper,
      base::SingleThreadTaskRunner* ui_task_runner,
      base::SequencedTaskRunner* worker_task_runner);
  virtual ~RemoteChangeProcessorOnWorker();

  virtual void PrepareForProcessRemoteChange(
      const storage::FileSystemURL& url,
      const PrepareChangeCallback& callback) OVERRIDE;
  virtual void ApplyRemoteChange(const FileChange& change,
                                 const base::FilePath& local_path,
                                 const storage::FileSystemURL& url,
                                 const SyncStatusCallback& callback) OVERRIDE;
  virtual void FinalizeRemoteSync(
      const storage::FileSystemURL& url,
      bool clear_local_changes,
      const base::Closure& completion_callback) OVERRIDE;
  virtual void RecordFakeLocalChange(
      const storage::FileSystemURL& url,
      const FileChange& change,
      const SyncStatusCallback& callback) OVERRIDE;

  void DetachFromSequence();

 private:
  base::WeakPtr<RemoteChangeProcessorWrapper> wrapper_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(RemoteChangeProcessorOnWorker);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_CHANGE_PROCESSOR_ON_WORKER_H_
