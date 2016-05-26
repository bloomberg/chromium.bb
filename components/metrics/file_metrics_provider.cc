// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/file_metrics_provider.h"

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/strings/string_piece.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"


namespace metrics {

// This structure stores all the information about the sources being monitored
// and their current reporting state.
struct FileMetricsProvider::SourceInfo {
  SourceInfo(SourceType source_type) : type(source_type) {}
  ~SourceInfo() {}

  // How to access this source (file/dir, atomic/active).
  const SourceType type;

  // Where on disk the directory is located. This will only be populated when
  // a directory is being monitored.
  base::FilePath directory;

  // Where on disk the file is located. If a directory is being monitored,
  // this will be updated for whatever file is being read.
  base::FilePath path;

  // Name used inside prefs to persistent metadata.
  std::string prefs_key;

  // The last-seen time of this source to detect change.
  base::Time last_seen;

  // Indicates if the data has been read out or not.
  bool read_complete = false;

  // Once a file has been recognized as needing to be read, it is |mapped|
  // into memory. The contents of that file may be copied to local |data|
  // so that access to it does not cause disk I/O.
  std::vector<uint8_t> data;
  std::unique_ptr<base::MemoryMappedFile> mapped;
  std::unique_ptr<base::PersistentHistogramAllocator> allocator;

  // Once a file has been processed, the actual releasing of the file must
  // be done on a thread capable of doing file I/O. These fields hold the
  // previously active objects that are to be released when possible.
  std::unique_ptr<base::MemoryMappedFile> mapped_to_release;
  std::unique_ptr<base::PersistentHistogramAllocator> allocator_to_release;

 private:
  DISALLOW_COPY_AND_ASSIGN(SourceInfo);
};

FileMetricsProvider::FileMetricsProvider(
    const scoped_refptr<base::TaskRunner>& task_runner,
    PrefService* local_state)
    : task_runner_(task_runner),
      pref_service_(local_state),
      weak_factory_(this) {
}

FileMetricsProvider::~FileMetricsProvider() {}

void FileMetricsProvider::RegisterSource(const base::FilePath& path,
                                         SourceType type,
                                         SourceAssociation source_association,
                                         const base::StringPiece prefs_key) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<SourceInfo> source(new SourceInfo(type));
  source->prefs_key = prefs_key.as_string();

  switch (source->type) {
    case SOURCE_HISTOGRAMS_ATOMIC_FILE:
      source->path = path;
      break;
    case SOURCE_HISTOGRAMS_ATOMIC_DIR:
      source->directory = path;
      break;
  }

  // |prefs_key| may be empty if the caller does not wish to persist the
  // state across instances of the program.
  if (pref_service_ && !prefs_key.empty()) {
    source->last_seen = base::Time::FromInternalValue(
        pref_service_->GetInt64(metrics::prefs::kMetricsLastSeenPrefix +
                                source->prefs_key));
  }

  switch (source_association) {
    case ASSOCIATE_CURRENT_RUN:
      sources_to_check_.push_back(std::move(source));
      break;
    case ASSOCIATE_PREVIOUS_RUN:
      DCHECK_EQ(SOURCE_HISTOGRAMS_ATOMIC_FILE, source->type);
      sources_for_previous_run_.push_back(std::move(source));
      break;
  }
}

// static
void FileMetricsProvider::RegisterPrefs(PrefRegistrySimple* prefs,
                                        const base::StringPiece prefs_key) {
  prefs->RegisterInt64Pref(metrics::prefs::kMetricsLastSeenPrefix +
                           prefs_key.as_string(), 0);
}

// static
void FileMetricsProvider::CheckAndMapNewMetricSourcesOnTaskRunner(
    SourceInfoList* sources) {
  // This method has all state information passed in |sources| and is intended
  // to run on a worker thread rather than the UI thread.
  for (std::unique_ptr<SourceInfo>& source : *sources) {
    AccessResult result = CheckAndMapNewMetrics(source.get());
    // Some results are not reported in order to keep the dashboard clean.
    if (result != ACCESS_RESULT_DOESNT_EXIST &&
        result != ACCESS_RESULT_NOT_MODIFIED) {
      UMA_HISTOGRAM_ENUMERATION(
          "UMA.FileMetricsProvider.AccessResult", result, ACCESS_RESULT_MAX);
    }

    switch (source->type) {
      case SOURCE_HISTOGRAMS_ATOMIC_FILE:
      case SOURCE_HISTOGRAMS_ATOMIC_DIR:
        // Remove the file if it's already been seen.
        if (result == ACCESS_RESULT_NOT_MODIFIED)
          base::DeleteFile(source->path, /*recursive=*/false);
        break;
    }
  }
}

// This method has all state information passed in |source| and is intended
// to run on a worker thread rather than the UI thread.
// static
FileMetricsProvider::AccessResult FileMetricsProvider::CheckAndMapNewMetrics(
    SourceInfo* source) {
  DCHECK(!source->mapped);
  DCHECK(!source->allocator);

  // Release any resources from a previous run.
  source->mapped_to_release.reset();
  source->allocator_to_release.reset();
  source->data.clear();
  source->read_complete = false;

  // If the source is a directory, look for files within it.
  if (!source->directory.empty() && !LocateNextFileInDirectory(source))
    return ACCESS_RESULT_DOESNT_EXIST;

  // Do basic validation on the file metadata.
  base::File::Info info;
  if (!base::GetFileInfo(source->path, &info))
    return ACCESS_RESULT_DOESNT_EXIST;

  if (info.is_directory || info.size == 0)
    return ACCESS_RESULT_INVALID_FILE;

  if (source->last_seen >= info.last_modified)
    return ACCESS_RESULT_NOT_MODIFIED;

  // A new file of metrics has been found. Open it with exclusive access and
  // map it into memory. The file is opened with "share delete" so it can
  // be later deleted while still open.
  // TODO(bcwhite): Make this open read/write when supported for "active".
  base::File file(source->path, base::File::FLAG_OPEN |
                                base::File::FLAG_READ |
                                base::File::FLAG_EXCLUSIVE_READ |
                                base::File::FLAG_SHARE_DELETE);
  if (!file.IsValid())
    return ACCESS_RESULT_NO_EXCLUSIVE_OPEN;
  source->mapped.reset(new base::MemoryMappedFile());
  if (!source->mapped->Initialize(std::move(file))) {
    source->mapped.reset();
    return ACCESS_RESULT_SYSTEM_MAP_FAILURE;
  }

  // Ensure any problems below don't occur repeatedly.
  source->last_seen = info.last_modified;

  // Test the validity of the file contents.
  if (!base::FilePersistentMemoryAllocator::IsFileAcceptable(*source->mapped,
                                                             true)) {
    source->mapped.reset();
    return ACCESS_RESULT_INVALID_CONTENTS;
  }

  switch (source->type) {
    case SOURCE_HISTOGRAMS_ATOMIC_FILE:
    case SOURCE_HISTOGRAMS_ATOMIC_DIR:
      // For an "atomic" file, copy the data into local memory but don't
      // release the file so that it is held open to prevent access by other
      // processes. The copy means all I/O is done on this thread instead of
      // the thread processing the data.
      source->data.assign(source->mapped->data(),
                          source->mapped->data() + source->mapped->length());
      break;
  }

  return ACCESS_RESULT_SUCCESS;
}

// static
bool FileMetricsProvider::LocateNextFileInDirectory(SourceInfo* source) {
  DCHECK_EQ(SOURCE_HISTOGRAMS_ATOMIC_DIR, source->type);
  DCHECK(!source->directory.empty());

  // Open the directory and find all the files, remembering the oldest that
  // has not been read. They can be removed and/or ignored if they're older
  // than the last-check time.
  base::Time oldest_file_time = base::Time::Now();
  base::FilePath oldest_file_path;
  base::FilePath file_path;
  int file_count = 0;
  int delete_count = 0;
  base::FileEnumerator file_iter(source->directory, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  for (file_path = file_iter.Next(); !file_path.empty();
       file_path = file_iter.Next()) {
    base::FileEnumerator::FileInfo file_info = file_iter.GetInfo();

    // Ignore directories and zero-sized files.
    if (file_info.IsDirectory() || file_info.GetSize() == 0)
      continue;

    // Ignore temporary files.
    base::FilePath::CharType first_character =
        file_path.BaseName().value().front();
    if (first_character == FILE_PATH_LITERAL('.') ||
        first_character == FILE_PATH_LITERAL('_')) {
      continue;
    }

    // Ignore non-PMA (Persistent Memory Allocator) files.
    if (file_path.Extension() !=
        base::PersistentMemoryAllocator::kFileExtension) {
      continue;
    }

    // Process real files.
    base::Time modified = file_info.GetLastModifiedTime();
    if (modified > source->last_seen) {
      // This file hasn't been read. Remember it if it is older than others.
      if (modified < oldest_file_time) {
        oldest_file_path = std::move(file_path);
        oldest_file_time = modified;
      }
      ++file_count;
    } else {
      // This file has been read. Try to delete it. Ignore any errors because
      // the file may be un-removeable by this process. It could, for example,
      // have been created by a privileged process like setup.exe. Even if it
      // is not removed, it will continue to be ignored bacuse of the older
      // modification time.
      base::DeleteFile(file_path, /*recursive=*/false);
      ++delete_count;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("UMA.FileMetricsProvider.DirectoryFiles",
                           file_count);
  UMA_HISTOGRAM_COUNTS_100("UMA.FileMetricsProvider.DeletedFiles",
                           delete_count);

  // Stop now if there are no files to read.
  if (oldest_file_path.empty())
    return false;

  // Set the active file to be the oldest modified file that has not yet
  // been read.
  source->path = std::move(oldest_file_path);
  return true;
}

// static
void FileMetricsProvider::DeleteFileWhenPossible(const base::FilePath& path) {
#if defined(OS_WIN)
  // Windows won't allow an open file to be removed from the filesystem so
  // schedule the file to be deleted as soon as it is closed. No access flag
  // (eg. FLAG_READ) is given as it would fail because the original read was
  // exclusive.
  base::File file(path, base::File::FLAG_OPEN |
                        base::File::FLAG_DELETE_ON_CLOSE);
#else
  // Posix won't allow opening a file without some access flag (eg. FLAG_READ)
  // but will allow one to be removed while open. Delete it the typical way.
  base::DeleteFile(path, /*recursive=*/false);
#endif
}

void FileMetricsProvider::ScheduleSourcesCheck() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (sources_to_check_.empty())
    return;

  // Create an independent list of sources for checking. This will be Owned()
  // by the reply call given to the task-runner, to be deleted when that call
  // has returned. It is also passed Unretained() to the task itself, safe
  // because that must complete before the reply runs.
  SourceInfoList* check_list = new SourceInfoList();
  std::swap(sources_to_check_, *check_list);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&FileMetricsProvider::CheckAndMapNewMetricSourcesOnTaskRunner,
                 base::Unretained(check_list)),
      base::Bind(&FileMetricsProvider::RecordSourcesChecked,
                 weak_factory_.GetWeakPtr(), base::Owned(check_list)));
}

void FileMetricsProvider::RecordHistogramSnapshotsFromSource(
    base::HistogramSnapshotManager* snapshot_manager,
    SourceInfo* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::PersistentHistogramAllocator::Iterator histogram_iter(
      source->allocator.get());

  int histogram_count = 0;
  while (true) {
    std::unique_ptr<base::HistogramBase> histogram = histogram_iter.GetNext();
    if (!histogram)
      break;
    switch (source->type) {
      case SOURCE_HISTOGRAMS_ATOMIC_FILE:
      case SOURCE_HISTOGRAMS_ATOMIC_DIR:
        snapshot_manager->PrepareFinalDeltaTakingOwnership(
            std::move(histogram));
        break;
#if 0  // Not yet available.
      case SOURCE_HISTOGRAMS_ACTIVE:
        snapshot_manager->PrepareDeltaTakingOwnership(std::move(histogram));
#endif
    }
    ++histogram_count;
  }

  DVLOG(1) << "Reported " << histogram_count << " histograms from "
           << source->path.value();
}

void FileMetricsProvider::CreateAllocatorForSource(SourceInfo* source) {
  DCHECK(source->mapped);
  DCHECK(!source->allocator);

  // File data was validated earlier. Files are not considered "untrusted"
  // as some processes might be (e.g. Renderer) so there's no need to check
  // again to try to thwart some malicious actor that may have modified the
  // data between then and now.
  if (source->data.empty()) {
    // Data hasn't been copied into memory. Create the allocator directly
    // on the memory-mapped file.
    // TODO(bcwhite): Make this do read/write when supported for "active".
    source->allocator.reset(new base::PersistentHistogramAllocator(
        base::WrapUnique(new base::FilePersistentMemoryAllocator(
            std::move(source->mapped), 0, 0, base::StringPiece(), true))));
  } else {
    // Data was copied from the mapped file into memory. Create an allocator
    // on the copy thus eliminating disk I/O during data access.
    source->allocator.reset(new base::PersistentHistogramAllocator(
        base::WrapUnique(new base::PersistentMemoryAllocator(
            &source->data[0], source->data.size(), 0, 0, base::StringPiece(),
            true))));
  }
}

void FileMetricsProvider::RecordSourcesChecked(SourceInfoList* checked) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Move each processed source to either the "to-read" list (for processing)
  // or the "to-check" list (for future checking).
  for (auto iter = checked->begin(); iter != checked->end();) {
    auto temp = iter++;
    const SourceInfo* source = temp->get();
    if (source->mapped)
      sources_to_read_.splice(sources_to_read_.end(), *checked, temp);
    else
      sources_to_check_.splice(sources_to_check_.end(), *checked, temp);
  }
}

void FileMetricsProvider::RecordSourceAsRead(SourceInfo* source) {
  DCHECK(thread_checker_.CalledOnValidThread());

  source->read_complete = true;

  // Persistently record the "last seen" timestamp of the source file to
  // ensure that the file is never read again unless it is modified again.
  if (pref_service_ && !source->prefs_key.empty()) {
    pref_service_->SetInt64(
        metrics::prefs::kMetricsLastSeenPrefix + source->prefs_key,
        source->last_seen.ToInternalValue());
  }

  switch (source->type) {
    case SOURCE_HISTOGRAMS_ATOMIC_FILE:
    case SOURCE_HISTOGRAMS_ATOMIC_DIR:
      // The actual release of the data is delayed until the next sources-
      // check so that the data remains valid in memory until it is fully
      // processed by the caller. These will be released during the next
      // ScheduleSourcesCheck() started by OnDidCreateMetricsLog() as soon
      // as the data is uploaded. This is also necessary because unmapping
      // must be done on a thread capable of file I/O.
      source->mapped_to_release = std::move(source->mapped);
      source->allocator_to_release = std::move(source->allocator);

      // Schedule the deletion of the file now that the data has been sent.
      // While recording the last-modified time prevents this process from
      // re-reading the data, deleting the file prevents other processes
      // (such as those with different profile directories or those run by
      // other users on the machine) from also reporting the contents of the
      // file.
      task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&FileMetricsProvider::DeleteFileWhenPossible,
                   source->path));
      break;
  }
}

void FileMetricsProvider::OnDidCreateMetricsLog() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Move finished metric sources back to list of monitored sources.
  for (auto iter = sources_to_read_.begin(); iter != sources_to_read_.end();) {
    auto temp = iter++;
    SourceInfo* source = temp->get();

    // The file has been fully processed once it is no longer "mapped" (even
    // though it may still be "mapped to release"). In practice, this only
    // happens with "atomic" files since "active" files may always acquire
    // more data to process.
    if (!source->mapped)
      sources_to_check_.splice(sources_to_check_.end(), sources_to_read_, temp);
  }

  // Schedule a check to see if there are new metrics to load. If so, they
  // will be reported during the next collection run after this one. The
  // check is run off of the worker-pool so as to not cause delays on the
  // main UI thread (which is currently where metric collection is done).
  ScheduleSourcesCheck();

  // Clear any data for initial metrics since they're always reported
  // before the first call to this method. It couldn't be released after
  // being reported in RecordInitialHistogramSnapshots because the data
  // will continue to be used by the caller after that method returns. Once
  // here, though, all actions to be done on the data have been completed.
#if DCHECK_IS_ON()
  for (const std::unique_ptr<SourceInfo>& source : sources_for_previous_run_)
    DCHECK(source->read_complete);
#endif
  sources_for_previous_run_.clear();
}

bool FileMetricsProvider::HasInitialStabilityMetrics() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Measure the total time spent checking all sources as well as the time
  // per individual file. This method is called during startup and thus blocks
  // the initial showing of the browser window so it's important to know the
  // total delay.
  SCOPED_UMA_HISTOGRAM_TIMER("UMA.FileMetricsProvider.InitialCheckTime.Total");

  // Check all sources for previous run to see if they need to be read.
  for (auto iter = sources_for_previous_run_.begin();
       iter != sources_for_previous_run_.end();) {
    SCOPED_UMA_HISTOGRAM_TIMER("UMA.FileMetricsProvider.InitialCheckTime.File");

    auto temp = iter++;
    SourceInfo* source = temp->get();

    // This would normally be done on a background I/O thread but there
    // hasn't been a chance to run any at the time this method is called.
    // Do the check in-line.
    AccessResult result = CheckAndMapNewMetrics(source);
    UMA_HISTOGRAM_ENUMERATION("UMA.FileMetricsProvider.InitialAccessResult",
                              result, ACCESS_RESULT_MAX);

    // If it couldn't be accessed, remove it from the list. There is only ever
    // one chance to record it so no point keeping it around for later. Also
    // mark it as having been read since uploading it with a future browser
    // run would associate it with the then-previous run which would no longer
    // be the run from which it came.
    if (result != ACCESS_RESULT_SUCCESS) {
      DCHECK(!source->mapped);
      DCHECK(!source->allocator);
      RecordSourceAsRead(source);
      sources_for_previous_run_.erase(temp);
    }
  }

  return !sources_for_previous_run_.empty();
}

void FileMetricsProvider::RecordHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Measure the total time spent processing all sources as well as the time
  // per individual file. This method is called on the UI thread so it's
  // important to know how much total "jank" may be introduced.
  SCOPED_UMA_HISTOGRAM_TIMER("UMA.FileMetricsProvider.SnapshotTime.Total");

  for (std::unique_ptr<SourceInfo>& source : sources_to_read_) {
    // Skip this source if the data has already been read.
    if (source->read_complete)
      continue;

    SCOPED_UMA_HISTOGRAM_TIMER("UMA.FileMetricsProvider.SnapshotTime.File");

    // The source needs to have an allocator attached to it in order to read
    // histograms out of it.
    CreateAllocatorForSource(source.get());
    DCHECK(source->allocator);

    // Dump all histograms contained within the source to the snapshot-manager.
    RecordHistogramSnapshotsFromSource(snapshot_manager, source.get());

    // Update the last-seen time so it isn't read again unless it changes.
    RecordSourceAsRead(source.get());
  }
}

void FileMetricsProvider::RecordInitialHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Measure the total time spent processing all sources as well as the time
  // per individual file. This method is called during startup and thus blocks
  // the initial showing of the browser window so it's important to know the
  // total delay.
  SCOPED_UMA_HISTOGRAM_TIMER(
      "UMA.FileMetricsProvider.InitialSnapshotTime.Total");

  for (const std::unique_ptr<SourceInfo>& source : sources_for_previous_run_) {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "UMA.FileMetricsProvider.InitialSnapshotTime.File");

    // The source needs to have an allocator attached to it in order to read
    // histograms out of it.
    DCHECK(!source->read_complete);
    DCHECK(source->mapped);
    CreateAllocatorForSource(source.get());
    DCHECK(source->allocator);

    // Dump all histograms contained within the source to the snapshot-manager.
    RecordHistogramSnapshotsFromSource(snapshot_manager, source.get());

    // Update the last-seen time so it isn't read again unless it changes.
    RecordSourceAsRead(source.get());
  }
}

}  // namespace metrics
