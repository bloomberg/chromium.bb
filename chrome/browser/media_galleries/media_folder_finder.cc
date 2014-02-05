// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_folder_finder.h"

#include <algorithm>
#include <set>

#include "base/files/file_enumerator.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/common/chrome_paths.h"
#include "chromeos/dbus/cros_disks_client.h"
#endif

typedef base::Callback<void(const std::vector<base::FilePath>& /*roots*/)>
    DefaultScanRootsCallback;
using content::BrowserThread;

namespace {

const int64 kMinimumImageSize = 200 * 1024;    // 200 KB
const int64 kMinimumAudioSize = 500 * 1024;    // 500 KB
const int64 kMinimumVideoSize = 1024 * 1024;   // 1 MB

bool IsValidScanPath(const base::FilePath& path) {
  return !path.empty() && path.IsAbsolute();
}

MediaGalleryScanFileType FilterPath(MediaPathFilter* filter,
                                    const base::FilePath& path) {
  DCHECK(IsValidScanPath(path));
  return filter->GetType(path);
}

bool FileMeetsSizeRequirement(bool requirement_met,
                              MediaGalleryScanFileType type,
                              int64 size) {
  if (requirement_met)
    return true;

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

void ScanFolderOnBlockingPool(
    const base::FilePath& path,
    const MediaFolderFinder::FilterCallback& filter_callback_,
    MediaGalleryScanResult* scan_result,
    std::vector<base::FilePath>* new_folders) {
  CHECK(IsValidScanPath(path));
  DCHECK(scan_result);
  DCHECK(new_folders);
  DCHECK(IsEmptyScanResult(*scan_result));

  bool folder_meets_size_requirement = false;
  base::FileEnumerator enumerator(
      path,
      false, /* recursive? */
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES
#if defined(OS_POSIX)
      | base::FileEnumerator::SHOW_SYM_LINKS  // show symlinks, not follow.
#endif
      );
  while (!enumerator.Next().empty()) {
    base::FileEnumerator::FileInfo file_info = enumerator.GetInfo();
    base::FilePath full_path = path.Append(file_info.GetName());
    if (MediaPathFilter::ShouldSkip(full_path))
      continue;

    if (file_info.IsDirectory()) {
      new_folders->push_back(full_path);
      continue;
    }
    MediaGalleryScanFileType type = filter_callback_.Run(full_path);
    if (type == MEDIA_GALLERY_SCAN_FILE_TYPE_UNKNOWN)
      continue;

    // Make sure there is at least 1 file above a size threshold.
    if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE)
      scan_result->image_count += 1;
    if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_AUDIO)
      scan_result->audio_count += 1;
    if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_VIDEO)
      scan_result->video_count += 1;
    folder_meets_size_requirement =
        FileMeetsSizeRequirement(folder_meets_size_requirement,
                                 type,
                                 file_info.GetSize());
  }
  if (!folder_meets_size_requirement) {
    scan_result->image_count = 0;
    scan_result->audio_count = 0;
    scan_result->video_count = 0;
  }
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
        type != StorageInfo::FIXED_MASS_STORAGE) {
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

MediaFolderFinder::MediaFolderFinder(
    const MediaFolderFinderResultsCallback& callback)
    : results_callback_(callback),
      scan_state_(SCAN_STATE_NOT_STARTED),
      has_roots_for_testing_(false),
      weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

MediaFolderFinder::~MediaFolderFinder() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (scan_state_ == SCAN_STATE_FINISHED)
    return;

  MediaFolderFinderResults empty_results;
  results_callback_.Run(false /* success? */, empty_results);
}

void MediaFolderFinder::StartScan() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (scan_state_ != SCAN_STATE_NOT_STARTED)
    return;

  scan_state_ = SCAN_STATE_STARTED;
  GetDefaultScanRoots(
      base::Bind(&MediaFolderFinder::OnInitialized, weak_factory_.GetWeakPtr()),
      has_roots_for_testing_,
      roots_for_testing_);
}

void MediaFolderFinder::SetRootsForTesting(
    const std::vector<base::FilePath>& roots) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(SCAN_STATE_NOT_STARTED, scan_state_);

  has_roots_for_testing_ = true;
  roots_for_testing_ = roots;
}

void MediaFolderFinder::OnInitialized(
    const std::vector<base::FilePath>& roots) {
  DCHECK_EQ(SCAN_STATE_STARTED, scan_state_);
  DCHECK(!token_.IsValid());
  DCHECK(filter_callback_.is_null());

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
  token_ = BrowserThread::GetBlockingPool()->GetSequenceToken();
  filter_callback_ = base::Bind(&FilterPath,
                                base::Owned(new MediaPathFilter()));
  ScanFolder();
}

void MediaFolderFinder::ScanFolder() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(SCAN_STATE_STARTED, scan_state_);

  if (folders_to_scan_.empty()) {
    scan_state_ = SCAN_STATE_FINISHED;
    results_callback_.Run(true /* success? */, results_);
    return;
  }

  DCHECK(token_.IsValid());
  DCHECK(!filter_callback_.is_null());

  base::FilePath folder_to_scan = folders_to_scan_.back();
  folders_to_scan_.pop_back();
  MediaGalleryScanResult* scan_result = new MediaGalleryScanResult();
  std::vector<base::FilePath>* new_folders = new std::vector<base::FilePath>();
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(token_);
  task_runner->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ScanFolderOnBlockingPool,
                 folder_to_scan,
                 filter_callback_,
                 base::Unretained(scan_result),
                 base::Unretained(new_folders)),
      base::Bind(&MediaFolderFinder::GotScanResults,
                 weak_factory_.GetWeakPtr(),
                 folder_to_scan,
                 base::Owned(scan_result),
                 base::Owned(new_folders)));
}

void MediaFolderFinder::GotScanResults(
    const base::FilePath& path,
    const MediaGalleryScanResult* scan_result,
    const std::vector<base::FilePath>* new_folders) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(SCAN_STATE_STARTED, scan_state_);
  DCHECK(!path.empty());
  DCHECK(scan_result);
  DCHECK(new_folders);
  CHECK(!ContainsKey(results_, path));

  if (!IsEmptyScanResult(*scan_result))
    results_[path] = *scan_result;

  // Push new folders to the |folders_to_scan_| in reverse order.
  std::copy(new_folders->rbegin(), new_folders->rend(),
            std::back_inserter(folders_to_scan_));

  ScanFolder();
}
