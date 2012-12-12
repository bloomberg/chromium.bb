// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_PREFETCHER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_PREFETCHER_H_

#include <set>
#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_observer.h"
#include "chrome/browser/chromeos/drive/drive_sync_client_observer.h"

class FilePath;

namespace drive {

class EventLogger;

// The parameters for DrivePrefetcher construction.
struct DrivePrefetcherOptions {
  DrivePrefetcherOptions();  // Sets the default values.

  int initial_prefetch_count;
  int64 prefetch_file_size_limit;
};

// DrivePrefetcher is used to observe and scan the Drive file system for
// maintaining the prioritized list of files to prefetch into the cache.
//
// All the methods (including ctor and dtor) must be called from UI thread.
class DrivePrefetcher : public DriveFileSystemObserver,
                        public DriveSyncClientObserver {
 public:
  DrivePrefetcher(DriveFileSystemInterface* file_system,
                  EventLogger* event_logger,
                  const DrivePrefetcherOptions& options);
  virtual ~DrivePrefetcher();

  // DriveFileSystemObserver overrides.
  virtual void OnInitialLoadFinished(DriveFileError error) OVERRIDE;
  virtual void OnDirectoryChanged(const FilePath& directory_path) OVERRIDE;

  // DriveSyncClientObserver overrides.
  virtual void OnSyncTaskStarted() OVERRIDE;
  virtual void OnSyncClientStopped() OVERRIDE;
  virtual void OnSyncClientIdle() OVERRIDE;

 private:
  // Initializes the internal data to keep the prefetch priority among files.
  void DoFullScan();

  // Fetches the file with the highest prefetch priority. If prefetching is
  // currently suspended, do nothing.
  void DoPrefetch();

  // Called when DoPrefetch is done.
  void OnPrefetchFinished(const std::string& resource_id,
                          DriveFileError error,
                          const FilePath& file_path,
                          const std::string& mime_type,
                          DriveFileType file_type);

  // Creates the |queue_| from the list of files with |latest_| timestamps.
  void ReconstructQueue();

  // Helper methods to traverse over the file system.
  void VisitFile(const DriveEntryProto& entry);
  void VisitDirectory(const FilePath& directory_path);
  void OnReadDirectory(const FilePath& directory_path,
                       DriveFileError error,
                       bool hide_hosted_documents,
                       scoped_ptr<DriveEntryProtoVector> entries);
  void OnReadDirectoryFinished();

  // Keeps the kNumberOfLatestFilesToKeepInCache latest files in the filesystem.
  typedef bool (*PrefetchPriorityComparator)(const DriveEntryProto&,
                                             const DriveEntryProto&);
  typedef std::set<DriveEntryProto, PrefetchPriorityComparator> LatestFileSet;
  LatestFileSet latest_files_;

  // The queue of files to fetch. Files with higher priority comes front.
  std::deque<std::string> queue_;

  // Number of in-flight |ExecuteOnePrefetch| calls that has not finished yet.
  int number_of_inflight_prefetches_;

  // Number of in-flight |VisitDirectory| calls that has not finished yet.
  int number_of_inflight_traversals_;

  // Indicates whether or not prefetching should be suspended, depending on
  // the command line flag and the SyncClient's activity.
  bool should_suspend_prefetch_;

  // Number of files to put into prefetch queue
  int initial_prefetch_count_;

  // The maximum file size for prefetched files. Files larger than the limit is
  // ignored from the prefetcher.
  int64 prefetch_file_size_limit_;

  // File system is owned by DriveSystemService.
  DriveFileSystemInterface* file_system_;

  // Event logger is owned by DriveSystemService.
  EventLogger* event_logger_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DrivePrefetcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DrivePrefetcher);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_PREFETCHER_H_
