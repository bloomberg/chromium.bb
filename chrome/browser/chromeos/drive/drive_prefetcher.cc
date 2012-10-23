// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_prefetcher.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {

const int kInitialPrefetchCount = 100;
const int64 kPrefetchFileSizeLimit = 10 << 20;  // 10MB

// Returns true if prefetching is disabled by a command line option.
bool IsPrefetchDisabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableDrivePrefetch);
}

}

DrivePrefetcherOptions::DrivePrefetcherOptions()
    : initial_prefetch_count(kInitialPrefetchCount),
      prefetch_file_size_limit(kPrefetchFileSizeLimit) {
}

DrivePrefetcher::DrivePrefetcher(DriveFileSystemInterface* file_system,
                                 const DrivePrefetcherOptions& options)
    : number_of_inflight_prefetches_(0),
      number_of_inflight_traversals_(0),
      should_suspend_prefetch_(true),
      initial_prefetch_count_(options.initial_prefetch_count),
      prefetch_file_size_limit_(options.prefetch_file_size_limit),
      file_system_(file_system),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->AddObserver(this);
}

DrivePrefetcher::~DrivePrefetcher() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->RemoveObserver(this);
}

void DrivePrefetcher::OnInitialLoadFinished(DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == DRIVE_FILE_OK)
    DoFullScan();
}

void DrivePrefetcher::OnDirectoryChanged(const FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kinaba): crbug.com/156270.
  // Update the list of latest files and the prefetch queue if needed.
}

void DrivePrefetcher::OnSyncTaskStarted() {
  should_suspend_prefetch_ = true;
}

void DrivePrefetcher::OnSyncClientStopped() {
  should_suspend_prefetch_ = true;
}

void DrivePrefetcher::OnSyncClientIdle() {
  should_suspend_prefetch_ = IsPrefetchDisabled();
  DoPrefetch();
}

void DrivePrefetcher::DoFullScan() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (IsPrefetchDisabled())
    return;

  FilePath root(util::ExtractDrivePath(util::GetDriveMountPointPath()));
  VisitDirectory(root);
}

void DrivePrefetcher::DoPrefetch() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (should_suspend_prefetch_ ||
      queue_.empty() ||
      number_of_inflight_prefetches_ > 0)
    return;

  std::string resource_id = queue_.front();
  queue_.pop_front();

  ++number_of_inflight_prefetches_;
  file_system_->GetFileByResourceId(
      resource_id,
      base::Bind(&DrivePrefetcher::OnPrefetchFinished,
                 weak_ptr_factory_.GetWeakPtr()),
      google_apis::GetContentCallback());
}

void DrivePrefetcher::OnPrefetchFinished(DriveFileError error,
                                         const FilePath& file_path,
                                         const std::string& mime_type,
                                         DriveFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != DRIVE_FILE_OK) {
    LOG(WARNING) << "Prefetch failed: " << error;
  }

  --number_of_inflight_prefetches_;
  DoPrefetch();  // Start next prefetch.
}

void DrivePrefetcher::ReconstructQueue() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Put the files with latest timestamp into the queue.
  queue_.clear();
  for (LatestFileSet::reverse_iterator it = latest_files_.rbegin();
      it != latest_files_.rend(); ++it) {
    queue_.push_back(it->second);
  }
}

void DrivePrefetcher::VisitFile(const DriveEntryProto& entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int64 last_access = entry.file_info().last_accessed();
  const std::string& resource_id = entry.resource_id();

  // Excessively large files will not be fetched.
  if (entry.file_info().size() > prefetch_file_size_limit_)
    return;

  // Remember the file in the set ordered by the |last_accessed| field.
  latest_files_.insert(std::make_pair(last_access, resource_id));
  // If the set become too big, forget the oldest entry.
  if (latest_files_.size() > static_cast<size_t>(initial_prefetch_count_))
    latest_files_.erase(latest_files_.begin());
}

void DrivePrefetcher::VisitDirectory(const FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ++number_of_inflight_traversals_;
  file_system_->ReadDirectoryByPath(
      directory_path,
      base::Bind(&DrivePrefetcher::OnReadDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path));
}

void DrivePrefetcher::OnReadDirectory(
    const FilePath& directory_path,
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
  if (number_of_inflight_traversals_ == 0) {
    ReconstructQueue();
    DoPrefetch();  // Start prefetching.
  }
}

}  // namespace drive
