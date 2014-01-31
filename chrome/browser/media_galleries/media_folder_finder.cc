// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_folder_finder.h"

#include "base/files/file_enumerator.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

bool IsValidScanPath(const base::FilePath& path) {
  return !path.empty() && path.IsAbsolute();
}

MediaGalleryScanFileType FilterPath(MediaPathFilter* filter,
                                    const base::FilePath& path) {
  DCHECK(IsValidScanPath(path));
  return filter->GetType(path);
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
    // TODO(thestig) Make sure there is at least 1 file above a size threshold:
    // images >= 200KB, videos >= 1MB, music >= 500KB.
    MediaGalleryScanFileType type = filter_callback_.Run(full_path);
    if (type == MEDIA_GALLERY_SCAN_FILE_TYPE_UNKNOWN)
      continue;

    if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE)
      scan_result->image_count += 1;
    if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_AUDIO)
      scan_result->audio_count += 1;
    if (type & MEDIA_GALLERY_SCAN_FILE_TYPE_VIDEO)
      scan_result->video_count += 1;
  }
}

}  // namespace

MediaFolderFinder::MediaFolderFinder(
    const std::vector<base::FilePath>& roots,
    const MediaFolderFinderResultsCallback& callback)
    : results_callback_(callback),
      scan_state_(SCAN_STATE_NOT_STARTED),
      weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < roots.size(); ++i) {
    const base::FilePath& path = roots[i];
    if (!IsValidScanPath(path))
      continue;
    // TODO(thestig) Check |path| for overlap with the rest of |roots|.
    folders_to_scan_.push(path);
  }
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

  DCHECK(!token_.IsValid());
  DCHECK(filter_callback_.is_null());
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

  base::FilePath folder_to_scan = folders_to_scan_.top();
  folders_to_scan_.pop();
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

  // Push new folders to the |folders_to_scan_| stack in reverse order.
  for (size_t i = new_folders->size(); i > 0; --i) {
    const base::FilePath& path_to_add = (*new_folders)[i - 1];
    folders_to_scan_.push(path_to_add);
  }

  ScanFolder();
}
