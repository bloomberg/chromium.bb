// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_ENTRY_REVERT_PERFORMER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_ENTRY_REVERT_PERFORMER_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "google_apis/drive/gdata_errorcode.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class FileResource;
}  // namespace google_apis

namespace drive {

class FileChange;
class JobScheduler;
class ResourceEntry;
struct ClientContext;

namespace file_system {
class OperationDelegate;
}  // namespace file_system

namespace internal {

class ResourceMetadata;

// This class is responsible to revert local changes of an entry.
class EntryRevertPerformer {
 public:
  EntryRevertPerformer(base::SequencedTaskRunner* blocking_task_runner,
                       file_system::OperationDelegate* delegate,
                       JobScheduler* scheduler,
                       ResourceMetadata* metadata);
  ~EntryRevertPerformer();

  // Requests the server for metadata of the entry specified by |local_id|
  // and overwrites the locally stored entry with it.
  // Invokes |callback| when finished with the result of the operation.
  // |callback| must not be null.
  void RevertEntry(const std::string& local_id,
                   const ClientContext& context,
                   const FileOperationCallback& callback);

 private:
  // Part of RevertEntry(). Called after local metadata look up.
  void RevertEntryAfterPrepare(const ClientContext& context,
                               const FileOperationCallback& callback,
                               scoped_ptr<ResourceEntry> entry,
                               FileError error);

  // Part of RevertEntry(). Called after GetFileResource is completed.
  void RevertEntryAfterGetFileResource(
      const FileOperationCallback& callback,
      const std::string& local_id,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::FileResource> entry);

  // Part of RevertEntry(). Called after local metadata is updated.
  void RevertEntryAfterFinishRevert(const FileOperationCallback& callback,
                                    const FileChange* changed_files,
                                    FileError error);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  file_system::OperationDelegate* delegate_;
  JobScheduler* scheduler_;
  ResourceMetadata* metadata_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EntryRevertPerformer> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(EntryRevertPerformer);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_ENTRY_REVERT_PERFORMER_H_
