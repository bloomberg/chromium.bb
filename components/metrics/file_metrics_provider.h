// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_FILE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_FILE_METRICS_PROVIDER_H_

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/metrics/metrics_provider.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class MemoryMappedFile;
class PersistentMemoryAllocator;
class TaskRunner;
}

namespace metrics {

// FileMetricsProvider gathers and logs histograms written to files on disk.
// Any number of files can be registered and will be polled once per upload
// cycle (at startup and periodically thereafter -- about every 30 minutes
// for desktop) for data to send.
class FileMetricsProvider : public metrics::MetricsProvider {
 public:
  enum FileType {
    // "Atomic" files are a collection of histograms that are written
    // completely in a single atomic operation (typically a write followed
    // by an atomic rename) and the file is never updated again except to
    // be replaced by a completely new set of histograms. This is the only
    // option that can be used if the file is not writeable by *this*
    // process.
    FILE_HISTOGRAMS_ATOMIC,

    // "Active" files may be open by one or more other processes and updated
    // at any time with new samples or new histograms. Such files may also be
    // inactive for any period of time only to be opened again and have new
    // data written to them. The file should probably never be deleted because
    // there would be no guarantee that the data has been reported.
    // TODO(bcwhite): Enable when read/write mem-mapped files are supported.
    //FILE_HISTOGRAMS_ACTIVE,
  };

  FileMetricsProvider(const scoped_refptr<base::TaskRunner>& task_runner,
                      PrefService* local_state);
  ~FileMetricsProvider() override;

  // Indicates a file to be monitored and how the file is used. Because some
  // metadata may persist across process restarts, preferences entries are
  // used based on the |prefs_key| name. Call RegisterPrefs() with the same
  // name to create the necessary keys in advance. Set |prefs_key| empty
  // if no persistence is required.
  void RegisterFile(const base::FilePath& path,
                    FileType type,
                    const base::StringPiece prefs_key);

  // Registers all necessary preferences for maintaining persistent state
  // about a monitored file across process restarts. The |prefs_key| is
  // typically the filename.
  static void RegisterPrefs(PrefRegistrySimple* prefs,
                            const base::StringPiece prefs_key);

 private:
  friend class FileMetricsProviderTest;
  FRIEND_TEST_ALL_PREFIXES(FileMetricsProviderTest, AccessMetrics);

  // The different results that can occur accessing a file.
  enum AccessResult {
    // File was successfully mapped.
    ACCESS_RESULT_SUCCESS,

    // File does not exist.
    ACCESS_RESULT_DOESNT_EXIST,

    // File exists but not modified since last read.
    ACCESS_RESULT_NOT_MODIFIED,

    // File is not valid: is a directory or zero-size.
    ACCESS_RESULT_INVALID_FILE,

    // System could not map file into memory.
    ACCESS_RESULT_SYSTEM_MAP_FAILURE,

    // File had invalid contents.
    ACCESS_RESULT_INVALID_CONTENTS,

    ACCESS_RESULT_MAX
  };

  // Information about files being monitored; defined and used exclusively
  // inside the .cc file.
  struct FileInfo;
  using FileInfoList = std::list<std::unique_ptr<FileInfo>>;

  // Checks a list of files (on a task-runner allowed to do I/O) to see if
  // any should be processed during the next histogram collection.
  static void CheckAndMapNewMetricFilesOnTaskRunner(FileInfoList* files);

  // Checks an individual file as part of CheckAndMapNewMetricFilesOnTaskRunner.
  static AccessResult CheckAndMapNewMetrics(FileInfo* file);

  // Creates a task to check all monitored files for updates.
  void ScheduleFilesCheck();

  // Creates a PersistentMemoryAllocator for a file that has been marked to
  // have its metrics collected.
  void CreateAllocatorForFile(FileInfo* file);

  // Records all histograms from a given file via a snapshot-manager.
  void RecordHistogramSnapshotsFromFile(
      base::HistogramSnapshotManager* snapshot_manager,
      FileInfo* file);

  // Takes a list of files checked by an external task and determines what
  // to do with each.
  void RecordFilesChecked(FileInfoList* checked);

  // Updates the persistent state information to show a file as being read.
  void RecordFileAsSeen(FileInfo* file);

  // metrics::MetricsDataProvider:
  void OnDidCreateMetricsLog() override;
  void RecordHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager) override;

  // A task-runner capable of performing I/O.
  scoped_refptr<base::TaskRunner> task_runner_;

  // A list of files not currently active that need to be checked for changes.
  FileInfoList files_to_check_;

  // A list of files that have data to be read and reported.
  FileInfoList files_to_read_;

  // The preferences-service used to store persistent state about files.
  PrefService* pref_service_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<FileMetricsProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_FILE_METRICS_PROVIDER_H_
