// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_folder_finder.h"

#include <algorithm>
#include <set>

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/common/chrome_paths.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/common/chrome_paths.h"
#include "chromeos/dbus/cros_disks_client.h"
#endif

using storage_monitor::StorageInfo;
using storage_monitor::StorageMonitor;

typedef base::Callback<void(const std::vector<base::FilePath>& /*roots*/)>
    DefaultScanRootsCallback;
using content::BrowserThread;

namespace {

const int64 kMinimumImageSize = 200 * 1024;    // 200 KB
const int64 kMinimumAudioSize = 500 * 1024;    // 500 KB
const int64 kMinimumVideoSize = 1024 * 1024;   // 1 MB

const int kPrunedPaths[] = {
#if defined(OS_WIN)
  base::DIR_IE_INTERNET_CACHE,
  base::DIR_PROGRAM_FILES,
  base::DIR_PROGRAM_FILESX86,
  base::DIR_WINDOWS,
#endif
#if defined(OS_MACOSX) && !defined(OS_IOS)
  chrome::DIR_USER_APPLICATIONS,
  chrome::DIR_USER_LIBRARY,
#endif
#if defined(OS_LINUX)
  base::DIR_CACHE,
#endif
#if defined(OS_WIN) || defined(OS_LINUX)
  base::DIR_TEMP,
#endif
};

bool IsValidScanPath(const base::FilePath& path) {
  return !path.empty() && path.IsAbsolute();
}

void CountScanResult(MediaGalleryScanFileType type,
                     MediaGalleryScanResult* scan_result) {
  if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE)
    scan_result->image_count += 1;
  if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_AUDIO)
    scan_result->audio_count += 1;
  if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_VIDEO)
    scan_result->video_count += 1;
}

bool FileMeetsSizeRequirement(MediaGalleryScanFileType type, int64 size) {
  if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE)
    if (size >= kMinimumImageSize)
      return true;
  if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_AUDIO)
    if (size >= kMinimumAudioSize)
      return true;
  if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_VIDEO)
    if (size >= kMinimumVideoSize)
      return true;
  return false;
}

// Return true if |path| should not be considered as the starting point for a
// media scan.
bool ShouldIgnoreScanRoot(const base::FilePath& path) {
#if defined(OS_MACOSX)
  // Scanning root is of little value.
  return (path.value() == "/");
#elif defined(OS_CHROMEOS)
  // Sanity check to make sure mount points are where they should be.
  base::FilePath mount_point =
      chromeos::CrosDisksClient::GetRemovableDiskMountPoint();
  return mount_point.IsParent(path);
#elif defined(OS_LINUX)
  // /media and /mnt are likely the only places with interesting mount points.
  if (StartsWithASCII(path.value(), "/media", true) ||
      StartsWithASCII(path.value(), "/mnt", true)) {
    return false;
  }
  return true;
#elif defined(OS_WIN)
  return false;
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

// Return a location that is likely to have user data to scan, if any.
base::FilePath GetPlatformSpecificDefaultScanRoot() {
  base::FilePath root;
#if defined(OS_CHROMEOS)
  PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS_SAFE, &root);
#elif defined(OS_MACOSX) || defined(OS_LINUX)
  PathService::Get(base::DIR_HOME, &root);
#elif defined(OS_WIN)
  // Nothing to add.
#else
  NOTIMPLEMENTED();
#endif
  return root;
}

// Find the likely locations with user media files and pass them to
// |callback|. Locations are platform specific.
void GetDefaultScanRoots(const DefaultScanRootsCallback& callback,
                         bool has_override,
                         const std::vector<base::FilePath>& override_paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (has_override) {
    callback.Run(override_paths);
    return;
  }

  StorageMonitor* monitor = StorageMonitor::GetInstance();
  DCHECK(monitor->IsInitialized());

  std::vector<base::FilePath> roots;
  std::vector<StorageInfo> storages = monitor->GetAllAvailableStorages();
  for (size_t i = 0; i < storages.size(); ++i) {
    StorageInfo::Type type;
    if (!StorageInfo::CrackDeviceId(storages[i].device_id(), &type, NULL) ||
        (type != StorageInfo::FIXED_MASS_STORAGE &&
         type != StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM)) {
      continue;
    }
    base::FilePath path(storages[i].location());
    if (ShouldIgnoreScanRoot(path))
      continue;
    roots.push_back(path);
  }

  base::FilePath platform_root = GetPlatformSpecificDefaultScanRoot();
  if (!platform_root.empty())
    roots.push_back(platform_root);
  callback.Run(roots);
}

}  // namespace

MediaFolderFinder::WorkerReply::WorkerReply() {}

MediaFolderFinder::WorkerReply::~WorkerReply() {}

// The Worker is created on the UI thread, but does all its work on a blocking
// SequencedTaskRunner.
class MediaFolderFinder::Worker {
 public:
  explicit Worker(const std::vector<base::FilePath>& graylisted_folders);
  ~Worker();

  // Scans |path| and return the results.
  WorkerReply ScanFolder(const base::FilePath& path);

 private:
  void MakeFolderPathsAbsolute();

  bool folder_paths_are_absolute_;
  std::vector<base::FilePath> graylisted_folders_;
  std::vector<base::FilePath> pruned_folders_;

  scoped_ptr<MediaPathFilter> filter_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

MediaFolderFinder::Worker::Worker(
    const std::vector<base::FilePath>& graylisted_folders)
    : folder_paths_are_absolute_(false),
      graylisted_folders_(graylisted_folders),
      filter_(new MediaPathFilter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (size_t i = 0; i < arraysize(kPrunedPaths); ++i) {
    base::FilePath path;
    if (PathService::Get(kPrunedPaths[i], &path))
      pruned_folders_.push_back(path);
  }

  sequence_checker_.DetachFromSequence();
}

MediaFolderFinder::Worker::~Worker() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

MediaFolderFinder::WorkerReply MediaFolderFinder::Worker::ScanFolder(
    const base::FilePath& path) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(IsValidScanPath(path));

  if (!folder_paths_are_absolute_)
    MakeFolderPathsAbsolute();

  WorkerReply reply;
  bool folder_meets_size_requirement = false;
  bool is_graylisted_folder = false;
  base::FilePath abspath = base::MakeAbsoluteFilePath(path);
  if (abspath.empty())
    return reply;

  for (size_t i = 0; i < graylisted_folders_.size(); ++i) {
    if (abspath == graylisted_folders_[i] ||
        abspath.IsParent(graylisted_folders_[i])) {
      is_graylisted_folder = true;
      break;
    }
  }

  base::FileEnumerator enumerator(
      path,
      false, /* recursive? */
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES
#if defined(OS_POSIX)
      | base::FileEnumerator::SHOW_SYM_LINKS  // show symlinks, not follow.
#endif
      );  // NOLINT
  while (!enumerator.Next().empty()) {
    base::FileEnumerator::FileInfo file_info = enumerator.GetInfo();
    base::FilePath full_path = path.Append(file_info.GetName());
    if (MediaPathFilter::ShouldSkip(full_path))
      continue;

    // Enumerating a directory.
    if (file_info.IsDirectory()) {
      bool is_pruned_folder = false;
      base::FilePath abs_full_path = base::MakeAbsoluteFilePath(full_path);
      if (abs_full_path.empty())
        continue;
      for (size_t i = 0; i < pruned_folders_.size(); ++i) {
        if (abs_full_path == pruned_folders_[i]) {
          is_pruned_folder = true;
          break;
        }
      }

      if (!is_pruned_folder)
        reply.new_folders.push_back(full_path);
      continue;
    }

    // Enumerating a file.
    //
    // Do not include scan results for graylisted folders.
    if (is_graylisted_folder)
      continue;

    MediaGalleryScanFileType type = filter_->GetType(full_path);
    if (type == MEDIA_GALLERY_SCAN_FILE_TYPE_UNKNOWN)
      continue;

    CountScanResult(type, &reply.scan_result);
    if (!folder_meets_size_requirement) {
      folder_meets_size_requirement =
          FileMeetsSizeRequirement(type, file_info.GetSize());
    }
  }
  // Make sure there is at least 1 file above a size threshold.
  if (!folder_meets_size_requirement)
    reply.scan_result = MediaGalleryScanResult();
  return reply;
}

void MediaFolderFinder::Worker::MakeFolderPathsAbsolute() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(!folder_paths_are_absolute_);
  folder_paths_are_absolute_ = true;

  std::vector<base::FilePath> abs_paths;
  for (size_t i = 0; i < graylisted_folders_.size(); ++i) {
    base::FilePath path = base::MakeAbsoluteFilePath(graylisted_folders_[i]);
    if (!path.empty())
      abs_paths.push_back(path);
  }
  graylisted_folders_ = abs_paths;
  abs_paths.clear();
  for (size_t i = 0; i < pruned_folders_.size(); ++i) {
    base::FilePath path = base::MakeAbsoluteFilePath(pruned_folders_[i]);
    if (!path.empty())
      abs_paths.push_back(path);
  }
  pruned_folders_ = abs_paths;
}

MediaFolderFinder::MediaFolderFinder(
    const MediaFolderFinderResultsCallback& callback)
    : results_callback_(callback),
      graylisted_folders_(
          extensions::file_system_api::GetGrayListedDirectories()),
      scan_state_(SCAN_STATE_NOT_STARTED),
      worker_(new Worker(graylisted_folders_)),
      has_roots_for_testing_(false),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  worker_task_runner_ = pool->GetSequencedTaskRunner(pool->GetSequenceToken());
}

MediaFolderFinder::~MediaFolderFinder() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  worker_task_runner_->DeleteSoon(FROM_HERE, worker_);

  if (scan_state_ == SCAN_STATE_FINISHED)
    return;

  MediaFolderFinderResults empty_results;
  results_callback_.Run(false /* success? */, empty_results);
}

void MediaFolderFinder::StartScan() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (scan_state_ != SCAN_STATE_NOT_STARTED)
    return;

  scan_state_ = SCAN_STATE_STARTED;
  GetDefaultScanRoots(
      base::Bind(&MediaFolderFinder::OnInitialized, weak_factory_.GetWeakPtr()),
      has_roots_for_testing_,
      roots_for_testing_);
}

const std::vector<base::FilePath>&
MediaFolderFinder::graylisted_folders() const {
  return graylisted_folders_;
}

void MediaFolderFinder::SetRootsForTesting(
    const std::vector<base::FilePath>& roots) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(SCAN_STATE_NOT_STARTED, scan_state_);

  has_roots_for_testing_ = true;
  roots_for_testing_ = roots;
}

void MediaFolderFinder::OnInitialized(
    const std::vector<base::FilePath>& roots) {
  DCHECK_EQ(SCAN_STATE_STARTED, scan_state_);

  std::set<base::FilePath> valid_roots;
  for (size_t i = 0; i < roots.size(); ++i) {
    // Skip if |path| is invalid or redundant.
    const base::FilePath& path = roots[i];
    if (!IsValidScanPath(path))
      continue;
    if (ContainsKey(valid_roots, path))
      continue;

    // Check for overlap.
    bool valid_roots_contains_path = false;
    std::vector<base::FilePath> overlapping_paths_to_remove;
    for (std::set<base::FilePath>::iterator it = valid_roots.begin();
         it != valid_roots.end(); ++it) {
      if (it->IsParent(path)) {
        valid_roots_contains_path = true;
        break;
      }
      const base::FilePath& other_path = *it;
      if (path.IsParent(other_path))
        overlapping_paths_to_remove.push_back(other_path);
    }
    if (valid_roots_contains_path)
      continue;
    // Remove anything |path| overlaps from |valid_roots|.
    for (size_t i = 0; i < overlapping_paths_to_remove.size(); ++i)
      valid_roots.erase(overlapping_paths_to_remove[i]);

    valid_roots.insert(path);
  }

  std::copy(valid_roots.begin(), valid_roots.end(),
            std::back_inserter(folders_to_scan_));
  ScanFolder();
}

void MediaFolderFinder::ScanFolder() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(SCAN_STATE_STARTED, scan_state_);

  if (folders_to_scan_.empty()) {
    scan_state_ = SCAN_STATE_FINISHED;
    results_callback_.Run(true /* success? */, results_);
    return;
  }

  base::FilePath folder_to_scan = folders_to_scan_.back();
  folders_to_scan_.pop_back();
  base::PostTaskAndReplyWithResult(
      worker_task_runner_, FROM_HERE,
      base::Bind(&Worker::ScanFolder,
                 base::Unretained(worker_),
                 folder_to_scan),
      base::Bind(&MediaFolderFinder::GotScanResults,
                 weak_factory_.GetWeakPtr(),
                 folder_to_scan));
}

void MediaFolderFinder::GotScanResults(const base::FilePath& path,
                                       const WorkerReply& reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(SCAN_STATE_STARTED, scan_state_);
  DCHECK(!path.empty());
  CHECK(!ContainsKey(results_, path));

  if (!IsEmptyScanResult(reply.scan_result))
    results_[path] = reply.scan_result;

  // Push new folders to the |folders_to_scan_| in reverse order.
  std::copy(reply.new_folders.rbegin(), reply.new_folders.rend(),
            std::back_inserter(folders_to_scan_));

  ScanFolder();
}
