// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/file_metrics_provider.h"

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"


namespace metrics {

// This structure stores all the information about the files being monitored
// and their current reporting state.
struct FileMetricsProvider::FileInfo {
  FileInfo(FileType file_type) : type(file_type) {}
  ~FileInfo() {}

  // How to access this file (atomic/active).
  const FileType type;

  // Where on disk the file is located.
  base::FilePath path;

  // Name used inside prefs to persistent metadata.
  std::string prefs_key;

  // The last-seen time of this file to detect change.
  base::Time last_seen;

  // Indicates if the data has been read out or not.
  bool read_complete = false;

  // Once a file has been recognized as needing to be read, it is |mapped|
  // into memory. If that file is "atomic" then the data from that file
  // will be copied to |data| and the mapped file released. If the file is
  // "active", it remains mapped and nothing is copied to local memory.
  std::vector<uint8_t> data;
  std::unique_ptr<base::MemoryMappedFile> mapped;
  std::unique_ptr<base::PersistentHistogramAllocator> allocator;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileInfo);
};

FileMetricsProvider::FileMetricsProvider(
    const scoped_refptr<base::TaskRunner>& task_runner,
    PrefService* local_state)
    : task_runner_(task_runner),
      pref_service_(local_state),
      weak_factory_(this) {
}

FileMetricsProvider::~FileMetricsProvider() {}

void FileMetricsProvider::RegisterFile(const base::FilePath& path,
                                       FileType type,
                                       const base::StringPiece prefs_key) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<FileInfo> file(new FileInfo(type));
  file->path = path;
  file->prefs_key = prefs_key.as_string();

  // |prefs_key| may be empty if the caller does not wish to persist the
  // state across instances of the program.
  if (pref_service_ && !prefs_key.empty()) {
    file->last_seen = base::Time::FromInternalValue(
        pref_service_->GetInt64(metrics::prefs::kMetricsLastSeenPrefix +
                                file->prefs_key));
  }

  files_to_check_.push_back(std::move(file));
}

// static
void FileMetricsProvider::RegisterPrefs(PrefRegistrySimple* prefs,
                                        const base::StringPiece prefs_key) {
  prefs->RegisterInt64Pref(metrics::prefs::kMetricsLastSeenPrefix +
                           prefs_key.as_string(), 0);
}

// static
void FileMetricsProvider::CheckAndMapNewMetricFilesOnTaskRunner(
    FileMetricsProvider::FileInfoList* files) {
  // This method has all state information passed in |files| and is intended
  // to run on a worker thread rather than the UI thread.
  for (std::unique_ptr<FileInfo>& file : *files) {
    AccessResult result = CheckAndMapNewMetrics(file.get());
    // Some results are not reported in order to keep the dashboard clean.
    if (result != ACCESS_RESULT_DOESNT_EXIST &&
        result != ACCESS_RESULT_NOT_MODIFIED) {
      UMA_HISTOGRAM_ENUMERATION(
          "UMA.FileMetricsProvider.AccessResult", result, ACCESS_RESULT_MAX);
    }
  }
}

// This method has all state information passed in |file| and is intended
// to run on a worker thread rather than the UI thread.
// static
FileMetricsProvider::AccessResult FileMetricsProvider::CheckAndMapNewMetrics(
    FileMetricsProvider::FileInfo* file) {
  DCHECK(!file->mapped);
  DCHECK(file->data.empty());

  base::File::Info info;
  if (!base::GetFileInfo(file->path, &info))
    return ACCESS_RESULT_DOESNT_EXIST;

  if (info.is_directory || info.size == 0)
    return ACCESS_RESULT_INVALID_FILE;

  if (file->last_seen >= info.last_modified)
    return ACCESS_RESULT_NOT_MODIFIED;

  // A new file of metrics has been found. Map it into memory.
  // TODO(bcwhite): Make this open read/write when supported for "active".
  file->mapped.reset(new base::MemoryMappedFile());
  if (!file->mapped->Initialize(file->path)) {
    file->mapped.reset();
    return ACCESS_RESULT_SYSTEM_MAP_FAILURE;
  }

  // Ensure any problems below don't occur repeatedly.
  file->last_seen = info.last_modified;

  // Test the validity of the file contents.
  if (!base::FilePersistentMemoryAllocator::IsFileAcceptable(*file->mapped))
    return ACCESS_RESULT_INVALID_CONTENTS;

  // For an "atomic" file, immediately copy the data into local memory and
  // release the file so that it is not held open.
  if (file->type == FILE_HISTOGRAMS_ATOMIC) {
    file->data.assign(file->mapped->data(),
                      file->mapped->data() + file->mapped->length());
    file->mapped.reset();
  }

  file->read_complete = false;
  return ACCESS_RESULT_SUCCESS;
}

void FileMetricsProvider::ScheduleFilesCheck() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (files_to_check_.empty())
    return;

  // Create an independent list of files for checking. This will be Owned()
  // by the reply call given to the task-runner, to be deleted when that call
  // has returned. It is also passed Unretained() to the task itself, safe
  // because that must complete before the reply runs.
  FileInfoList* check_list = new FileInfoList();
  std::swap(files_to_check_, *check_list);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&FileMetricsProvider::CheckAndMapNewMetricFilesOnTaskRunner,
                 base::Unretained(check_list)),
      base::Bind(&FileMetricsProvider::RecordFilesChecked,
                 weak_factory_.GetWeakPtr(), base::Owned(check_list)));
}

void FileMetricsProvider::RecordHistogramSnapshotsFromFile(
    base::HistogramSnapshotManager* snapshot_manager,
    FileInfo* file) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::PersistentHistogramAllocator::Iterator histogram_iter(
      file->allocator.get());

  int histogram_count = 0;
  while (true) {
    std::unique_ptr<base::HistogramBase> histogram = histogram_iter.GetNext();
    if (!histogram)
      break;
    if (file->type == FILE_HISTOGRAMS_ATOMIC)
      snapshot_manager->PrepareAbsoluteTakingOwnership(std::move(histogram));
    else
      snapshot_manager->PrepareDeltaTakingOwnership(std::move(histogram));
    ++histogram_count;
  }

  DVLOG(1) << "Reported " << histogram_count << " histograms from "
           << file->path.value();
}

void FileMetricsProvider::CreateAllocatorForFile(FileInfo* file) {
  DCHECK(!file->allocator);

  // File data was validated earlier. Files are not considered "untrusted"
  // as some processes might be (e.g. Renderer) so there's no need to check
  // again to try to thwart some malicious actor that may have modified the
  // data between then and now.
  if (file->mapped) {
    DCHECK(file->data.empty());
    // TODO(bcwhite): Make this do read/write when supported for "active".
    file->allocator.reset(new base::PersistentHistogramAllocator(
        base::WrapUnique(new base::FilePersistentMemoryAllocator(
            std::move(file->mapped), 0, ""))));
  } else {
    DCHECK(!file->mapped);
    file->allocator.reset(new base::PersistentHistogramAllocator(
        base::WrapUnique(new base::PersistentMemoryAllocator(
            &file->data[0], file->data.size(), 0, 0, "", true))));
  }
}

void FileMetricsProvider::RecordFilesChecked(FileInfoList* checked) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Move each processed file to either the "to-read" list (for processing) or
  // the "to-check" list (for future checking).
  for (auto iter = checked->begin(); iter != checked->end();) {
    auto temp = iter++;
    const FileInfo* file = temp->get();
    if (file->mapped || !file->data.empty())
      files_to_read_.splice(files_to_read_.end(), *checked, temp);
    else
      files_to_check_.splice(files_to_check_.end(), *checked, temp);
  }
}

void FileMetricsProvider::RecordFileAsSeen(FileInfo* file) {
  DCHECK(thread_checker_.CalledOnValidThread());

  file->read_complete = true;
  if (pref_service_ && !file->prefs_key.empty()) {
    pref_service_->SetInt64(metrics::prefs::kMetricsLastSeenPrefix +
                            file->prefs_key,
                            file->last_seen.ToInternalValue());
  }
}

void FileMetricsProvider::OnDidCreateMetricsLog() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Move finished metric files back to list of monitored files.
  for (auto iter = files_to_read_.begin(); iter != files_to_read_.end();) {
    auto temp = iter++;
    FileInfo* file = temp->get();

    // Atomic files are read once and then ignored unless they change.
    if (file->type == FILE_HISTOGRAMS_ATOMIC && file->read_complete) {
      DCHECK(!file->mapped);
      file->allocator.reset();
      file->data.clear();
    }

    if (!file->allocator && !file->mapped && file->data.empty())
      files_to_check_.splice(files_to_check_.end(), files_to_read_, temp);
  }

  // Schedule a check to see if there are new metrics to load. If so, they
  // will be reported during the next collection run after this one. The
  // check is run off of the worker-pool so as to not cause delays on the
  // main UI thread (which is currently where metric collection is done).
  ScheduleFilesCheck();
}

void FileMetricsProvider::RecordHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (std::unique_ptr<FileInfo>& file : files_to_read_) {
    // Skip this file if the data has already been read.
    if (file->read_complete)
      continue;

    // If the file is mapped or loaded then it needs to have an allocator
    // attached to it in order to read histograms out of it.
    if (file->mapped || !file->data.empty())
      CreateAllocatorForFile(file.get());

    // A file should not be under "files to read" unless it has an allocator
    // or is memory-mapped (at which point it will have received an allocator
    // above). However, if this method gets called twice before the scheduled-
    // files-check has a chance to clean up, this may trigger. This also
    // catches the case where creating an allocator from the file has failed.
    if (!file->allocator)
      continue;

    // Dump all histograms contained within the file to the snapshot-manager.
    RecordHistogramSnapshotsFromFile(snapshot_manager, file.get());

    // Update the last-seen time so it isn't read again unless it changes.
    RecordFileAsSeen(file.get());
  }
}

}  // namespace metrics
