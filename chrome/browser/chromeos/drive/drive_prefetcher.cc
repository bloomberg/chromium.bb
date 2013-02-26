// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_prefetcher.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/event_logger.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {

const int kInitialPrefetchCount = 100;
const int64 kPrefetchFileSizeLimit = 10 << 20;  // 10MB

// Returns true if prefetching is enabled by a command line option.
bool IsPrefetchEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDrivePrefetch);
}

// Returns true if |left| has lower priority than |right|.
bool ComparePrefetchPriority(const DriveEntryProto& left,
                             const DriveEntryProto& right) {
  // First, compare last access time. The older entry has less priority.
  if (left.file_info().last_accessed() != right.file_info().last_accessed())
    return left.file_info().last_accessed() < right.file_info().last_accessed();

  // When the entries have the same last access time (which happens quite often
  // because Drive server doesn't set the field until an entry is viewed via
  // drive.google.com), we use last modified time as the tie breaker.
  if (left.file_info().last_modified() != right.file_info().last_modified())
    return left.file_info().last_modified() < right.file_info().last_modified();

  // Two entries have the same priority. To make this function a valid
  // comparator for std::set, we need to differentiate them anyhow.
  return left.resource_id() < right.resource_id();
}

}

DrivePrefetcherOptions::DrivePrefetcherOptions()
    : initial_prefetch_count(kInitialPrefetchCount),
      prefetch_file_size_limit(kPrefetchFileSizeLimit) {
}

DrivePrefetcher::DrivePrefetcher(DriveFileSystemInterface* file_system,
                                 EventLogger* event_logger,
                                 const DrivePrefetcherOptions& options)
    : latest_files_(&ComparePrefetchPriority),
      number_of_inflight_traversals_(0),
      initial_prefetch_count_(options.initial_prefetch_count),
      prefetch_file_size_limit_(options.prefetch_file_size_limit),
      file_system_(file_system),
      event_logger_(event_logger),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The flag controls whether or not the prefetch observe the file system. When
  // it is disabled, no event (except the direct call of OnInitialLoadFinished
  // in the unit test code) will trigger the prefetcher.
  if (IsPrefetchEnabled())
    file_system_->AddObserver(this);
}

DrivePrefetcher::~DrivePrefetcher() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (IsPrefetchEnabled())
    file_system_->RemoveObserver(this);
}

void DrivePrefetcher::OnInitialLoadFinished(DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == DRIVE_FILE_OK)
    StartPrefetcherCycle();
}

void DrivePrefetcher::OnDirectoryChanged(const base::FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kinaba): crbug.com/156270.
  // Update the list of latest files and the prefetch queue if needed.
}

void DrivePrefetcher::StartPrefetcherCycle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Scans the filesystem. When it is finished, DoPrefetch() will be called.
  base::FilePath root(util::ExtractDrivePath(util::GetDriveMountPointPath()));
  VisitDirectory(root);
}

void DrivePrefetcher::DoPrefetch() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (LatestFileSet::reverse_iterator it = latest_files_.rbegin();
      it != latest_files_.rend(); ++it) {
    const std::string& resource_id = it->resource_id();
    event_logger_->Log("Prefetcher: Enqueue prefetching " + resource_id);
    file_system_->GetFileByResourceId(
        resource_id,
        DriveClientContext(PREFETCH),
        base::Bind(&DrivePrefetcher::OnPrefetchFinished,
                   weak_ptr_factory_.GetWeakPtr(),
                   resource_id),
        google_apis::GetContentCallback());
  }
}

void DrivePrefetcher::OnPrefetchFinished(const std::string& resource_id,
                                         DriveFileError error,
                                         const base::FilePath& file_path,
                                         const std::string& mime_type,
                                         DriveFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (error != DRIVE_FILE_OK)
    LOG(WARNING) << "Prefetch failed: " << error;
  event_logger_->Log(base::StringPrintf("Prefetcher: Finish fetching (%s) %s",
                                        DriveFileErrorToString(error).c_str(),
                                        resource_id.c_str()));
}

void DrivePrefetcher::VisitFile(const DriveEntryProto& entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Excessively large files will not be fetched.
  if (entry.file_info().size() > prefetch_file_size_limit_)
    return;

  // Remember the file in the set ordered by the |last_accessed| field.
  latest_files_.insert(entry);
  // If the set become too big, forget the oldest entry.
  if (latest_files_.size() > static_cast<size_t>(initial_prefetch_count_))
    latest_files_.erase(latest_files_.begin());
}

void DrivePrefetcher::VisitDirectory(const base::FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ++number_of_inflight_traversals_;
  file_system_->ReadDirectoryByPath(
      directory_path,
      base::Bind(&DrivePrefetcher::OnReadDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path));
}

void DrivePrefetcher::OnReadDirectory(
    const base::FilePath& directory_path,
    DriveFileError error,
    bool hide_hosted_documents,
    scoped_ptr<DriveEntryProtoVector> entries) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != DRIVE_FILE_OK) {
    LOG(WARNING) << "Directory cannot be scanned by prefetcher: "
                 << directory_path.value();
    OnReadDirectoryFinished();
    return;
  }

  // TODO(kinaba): if entries->size() is so big and it does not contain any
  // directories, the loop below may run on UI thread long time. Consider
  // splitting it into a smaller asynchronous tasks.
  for (size_t i = 0; i < entries->size(); ++i) {
    const DriveEntryProto& entry = (*entries)[i];

    if (entry.file_info().is_directory()) {
      VisitDirectory(directory_path.Append(entry.base_name()));
    } else if (entry.has_file_specific_info() &&
               !entry.file_specific_info().is_hosted_document()) {
      VisitFile(entry);
    }
  }

  OnReadDirectoryFinished();
}

void DrivePrefetcher::OnReadDirectoryFinished() {
  DCHECK(number_of_inflight_traversals_ > 0);

  --number_of_inflight_traversals_;
  if (number_of_inflight_traversals_ == 0)
    DoPrefetch();  // Start prefetching.
}

}  // namespace drive
