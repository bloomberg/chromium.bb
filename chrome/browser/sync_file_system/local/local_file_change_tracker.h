// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_CHANGE_TRACKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_CHANGE_TRACKER_H_

#include <deque>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "webkit/browser/fileapi/file_observers.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {
class FileSystemContext;
class FileSystemURL;
}

namespace leveldb {
class Env;
}

namespace sync_file_system {

// Tracks local file changes for cloud-backed file systems.
// All methods must be called on the file_task_runner given to the constructor.
// Owned by FileSystemContext.
class LocalFileChangeTracker
    : public fileapi::FileUpdateObserver,
      public fileapi::FileChangeObserver {
 public:
  // |file_task_runner| must be the one where the observee file operations run.
  // (So that we can make sure DB operations are done before actual update
  // happens)
  LocalFileChangeTracker(const base::FilePath& base_path,
                         leveldb::Env* env_override,
                         base::SequencedTaskRunner* file_task_runner);
  virtual ~LocalFileChangeTracker();

  // FileUpdateObserver overrides.
  virtual void OnStartUpdate(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnUpdate(
      const fileapi::FileSystemURL& url, int64 delta) OVERRIDE {}
  virtual void OnEndUpdate(const fileapi::FileSystemURL& url) OVERRIDE;

  // FileChangeObserver overrides.
  virtual void OnCreateFile(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnCreateFileFrom(const fileapi::FileSystemURL& url,
                                const fileapi::FileSystemURL& src) OVERRIDE;
  virtual void OnRemoveFile(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnModifyFile(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnCreateDirectory(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnRemoveDirectory(const fileapi::FileSystemURL& url) OVERRIDE;

  // Retrieves an array of |url| which have more than one pending changes.
  // If |max_urls| is non-zero (recommended in production code) this
  // returns URLs up to the number from the ones that have smallest
  // change_seq numbers (i.e. older changes).
  void GetNextChangedURLs(std::deque<fileapi::FileSystemURL>* urls,
                          int max_urls);

  // Returns all changes recorded for the given |url|.
  // Note that this also returns demoted changes.
  // This should be called after writing is disabled.
  void GetChangesForURL(const fileapi::FileSystemURL& url,
                        FileChangeList* changes);

  // Clears the pending changes recorded in this tracker for |url|.
  void ClearChangesForURL(const fileapi::FileSystemURL& url);

  // Creates a fresh (empty) in-memory record for |url|.
  // Note that new changes are recorded to the mirror too.
  void CreateFreshMirrorForURL(const fileapi::FileSystemURL& url);

  // Removes a mirror for |url|, and commits the change status to database.
  void RemoveMirrorAndCommitChangesForURL(const fileapi::FileSystemURL& url);

  // Resets the changes to the ones recorded in mirror for |url|, and
  // commits the updated change status to database.
  void ResetToMirrorAndCommitChangesForURL(const fileapi::FileSystemURL& url);

  // Re-inserts changes into the separate demoted_changes_ queue. They won't
  // be fetched by GetNextChangedURLs() unless PromoteDemotedChanges() is
  // called.
  void DemoteChangesForURL(const fileapi::FileSystemURL& url);

  // Promotes demoted changes for |url| to the normal queue.
  void PromoteDemotedChangesForURL(const fileapi::FileSystemURL& url);

  // Promotes all demoted changes to the normal queue. Returns true if it has
  // promoted any changes.
  bool PromoteDemotedChanges();

  // Called by FileSyncService at the startup time to restore last dirty changes
  // left after the last shutdown (if any).
  SyncStatusCode Initialize(fileapi::FileSystemContext* file_system_context);

  // Resets all the changes recorded for the given |origin| and |type|.
  // TODO(kinuko,nhiroki): Ideally this should be automatically called in
  // DeleteFileSystem via QuotaUtil::DeleteOriginDataOnFileThread.
  void ResetForFileSystem(const GURL& origin, fileapi::FileSystemType type);

  // This method is (exceptionally) thread-safe.
  int64 num_changes() const {
    base::AutoLock lock(num_changes_lock_);
    return num_changes_;
  }

 private:
  class TrackerDB;
  friend class CannedSyncableFileSystem;
  friend class LocalFileChangeTrackerTest;
  friend class LocalFileSyncContext;
  friend class LocalFileSyncContextTest;
  friend class SyncableFileSystemTest;

  struct ChangeInfo {
    ChangeInfo();
    ~ChangeInfo();
    FileChangeList change_list;
    int64 change_seq;
  };

  typedef std::map<fileapi::FileSystemURL, ChangeInfo,
      fileapi::FileSystemURL::Comparator>
          FileChangeMap;
  typedef std::map<int64, fileapi::FileSystemURL> ChangeSeqMap;

  void UpdateNumChanges();

  // This does mostly same as calling GetNextChangedURLs with max_url=0
  // except that it returns urls in set rather than in deque.
  // Used only in testings.
  void GetAllChangedURLs(fileapi::FileSystemURLSet* urls);

  // Used only in testings.
  void DropAllChanges();

  // Database related methods.
  SyncStatusCode MarkDirtyOnDatabase(const fileapi::FileSystemURL& url);
  SyncStatusCode ClearDirtyOnDatabase(const fileapi::FileSystemURL& url);

  SyncStatusCode CollectLastDirtyChanges(
      fileapi::FileSystemContext* file_system_context);
  void RecordChange(const fileapi::FileSystemURL& url,
                    const FileChange& change);

  static void RecordChangeToChangeMaps(const fileapi::FileSystemURL& url,
                                       const FileChange& change,
                                       int change_seq,
                                       FileChangeMap* changes,
                                       ChangeSeqMap* change_seqs);

  bool initialized_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  FileChangeMap changes_;
  ChangeSeqMap change_seqs_;

  FileChangeMap mirror_changes_;  // For mirrors.
  FileChangeMap demoted_changes_;  // For demoted changes.

  scoped_ptr<TrackerDB> tracker_db_;

  // Change sequence number. Briefly gives a hint about the order of changes,
  // but they are updated when a new change comes on the same file (as
  // well as Drive's changestamps).
  int64 current_change_seq_;

  // This can be accessed on any threads (with num_changes_lock_).
  int64 num_changes_;
  mutable base::Lock num_changes_lock_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileChangeTracker);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_CHANGE_TRACKER_H_
