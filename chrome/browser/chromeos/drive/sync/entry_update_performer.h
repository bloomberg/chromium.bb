// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_ENTRY_UPDATE_PERFORMER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_ENTRY_UPDATE_PERFORMER_H_

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
class ResourceEntry;
}  // namespace google_apis

namespace drive {

class JobScheduler;
class ResourceEntry;

namespace file_system {
class OperationObserver;
}  // namespace file_system

namespace internal {

class EntryRevertPerformer;
class RemovePerformer;
class ResourceMetadata;

// This class is responsible to perform server side update of an entry.
class EntryUpdatePerformer {
 public:
  EntryUpdatePerformer(base::SequencedTaskRunner* blocking_task_runner,
                       file_system::OperationObserver* observer,
                       JobScheduler* scheduler,
                       ResourceMetadata* metadata);
  ~EntryUpdatePerformer();

  // Requests the server to update the metadata of the entry specified by
  // |local_id| with the locally stored one.
  // Invokes |callback| when finished with the result of the operation.
  // |callback| must not be null.
  void UpdateEntry(const std::string& local_id,
                   const FileOperationCallback& callback);

 private:
  // Part of UpdateEntry(). Called after local metadata look up.
  void UpdateEntryAfterPrepare(const FileOperationCallback& callback,
                               scoped_ptr<ResourceEntry> entry,
                               scoped_ptr<ResourceEntry> parent_entry,
                               FileError error);

  // Part of UpdateEntry(). Called after UpdateResource is completed.
  void UpdateEntryAfterUpdateResource(
      const FileOperationCallback& callback,
      const std::string& local_id,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  JobScheduler* scheduler_;
  ResourceMetadata* metadata_;
  scoped_ptr<RemovePerformer> remove_performer_;
  scoped_ptr<EntryRevertPerformer> entry_revert_performer_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EntryUpdatePerformer> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(EntryUpdatePerformer);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_ENTRY_UPDATE_PERFORMER_H_
