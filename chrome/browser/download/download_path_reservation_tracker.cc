// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_path_reservation_tracker.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_item.h"

using content::BrowserThread;
using content::DownloadId;
using content::DownloadItem;

namespace {

// Observes a DownloadItem for changes to its target path and state. Updates or
// revokes associated download path reservations as necessary.
class DownloadItemObserver : public DownloadItem::Observer {
 public:
  DownloadItemObserver(DownloadItem& download_item,
                       base::Closure revoke,
                       base::Callback<void(const FilePath&)> update);

 private:
  virtual ~DownloadItemObserver();

  // DownloadItem::Observer
  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(DownloadItem* download) OVERRIDE;

  DownloadItem& download_item_;

  // Last known target path for the download.
  FilePath last_target_path_;

  // Callback to invoke to revoke the path reseration.
  base::Closure revoke_callback_;

  // Callback to invoke to update the path reservation.
  base::Callback<void(const FilePath&)> update_callback_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemObserver);
};

DownloadItemObserver::DownloadItemObserver(
    DownloadItem& download_item,
    base::Closure revoke,
    base::Callback<void(const FilePath&)> update)
    : download_item_(download_item),
      last_target_path_(download_item.GetTargetFilePath()),
      revoke_callback_(revoke),
      update_callback_(update) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  download_item_.AddObserver(this);
}

DownloadItemObserver::~DownloadItemObserver() {
  download_item_.RemoveObserver(this);
}

void DownloadItemObserver::OnDownloadUpdated(DownloadItem* download) {
  switch (download->GetState()) {
    case DownloadItem::IN_PROGRESS: {
      // Update the reservation.
      FilePath new_target_path = download->GetTargetFilePath();
      if (new_target_path != last_target_path_) {
        BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                                base::Bind(update_callback_, new_target_path));
        last_target_path_ = new_target_path;
      }
      break;
    }

    case DownloadItem::COMPLETE:
      // If the download is complete, then it has already been renamed to the
      // final name. The existence of the file on disk is sufficient to prevent
      // conflicts from now on.

    case DownloadItem::CANCELLED:
      // We no longer need the reservation if the download is being removed.

    case DownloadItem::REMOVING:
      // Ditto, but this case shouldn't happen in practice. We should have
      // received another notification beforehand.

    case DownloadItem::INTERRUPTED:
      // The download filename will need to be re-generated when the download is
      // restarted. Holding on to the reservation now would prevent the name
      // from being used for a subsequent retry attempt.

      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, revoke_callback_);
      delete this;
      break;

    case DownloadItem::MAX_DOWNLOAD_STATE:
      // Compiler appeasement.
      NOTREACHED();
  }
}

void DownloadItemObserver::OnDownloadOpened(DownloadItem* download) {
  // We shouldn't be tracking reservations for a download that has been
  // externally opened. The tracker should have detached itself when the
  // download was complete.
}

}  // namespace

// static
void DownloadPathReservationTracker::GetReservedPath(
    DownloadItem& download_item,
    const FilePath& target_path,
    const FilePath& default_path,
    bool uniquify_path,
    const ReservedPathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadPathReservationTracker* tracker =
      DownloadPathReservationTracker::GetInstance();

  // Attach an observer to the download item so that we know when the target
  // path changes and/or the download is no longer active.
  new DownloadItemObserver(
      download_item,
      base::Bind(&DownloadPathReservationTracker::Revoke,
                 base::Unretained(tracker),
                 download_item.GetGlobalId()),
      base::Bind(&DownloadPathReservationTracker::Update,
                 base::Unretained(tracker),
                 download_item.GetGlobalId()));
  // DownloadItemObserver deletes itself.

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadPathReservationTracker::ReserveInternal,
                 base::Unretained(tracker), download_item.GetGlobalId(),
                 target_path, default_path, uniquify_path, callback));
}

DownloadPathReservationTracker::DownloadPathReservationTracker() {
}

DownloadPathReservationTracker::~DownloadPathReservationTracker() {
  DCHECK_EQ(0u, reservations_.size());
}

void DownloadPathReservationTracker::ReserveInternal(
    DownloadId download_id,
    const FilePath& suggested_path,
    const FilePath& default_download_path,
    bool should_uniquify,
    const ReservedPathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(download_id.IsValid());
  DCHECK(!ContainsKey(reservations_, download_id));
  DCHECK(suggested_path.IsAbsolute());

  FilePath target_path(suggested_path.NormalizePathSeparators());
  bool is_path_writeable = true;
  bool has_conflicts = false;

  // Create the default download path if it doesn't already exist and is where
  // we are going to create the downloaded file. |target_path| might point
  // elsewhere if this was a programmatic download.
  if (!default_download_path.empty() &&
      default_download_path == target_path.DirName() &&
      !file_util::DirectoryExists(default_download_path)) {
    file_util::CreateDirectory(default_download_path);
  }

  // Check writability of the suggested path. If we can't write to it, default
  // to the user's "My Documents" directory. We'll prompt them in this case.
  FilePath dir = target_path.DirName();
  FilePath filename = target_path.BaseName();
  if (!file_util::PathIsWritable(dir)) {
    DVLOG(1) << "Unable to write to directory \"" << dir.value() << "\"";
    is_path_writeable = false;
    PathService::Get(chrome::DIR_USER_DOCUMENTS, &dir);
    target_path = dir.Append(filename);
  }

  if (is_path_writeable && should_uniquify && IsPathInUse(target_path)) {
    has_conflicts = true;
    for (int uniquifier = 1; uniquifier <= kMaxUniqueFiles; ++uniquifier) {
      FilePath path_to_check(target_path.InsertBeforeExtensionASCII(
          StringPrintf(" (%d)", uniquifier)));
      if (!IsPathInUse(path_to_check)) {
        target_path = path_to_check;
        has_conflicts = false;
        break;
      }
    }
  }
  reservations_[download_id] = target_path;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, target_path, (is_path_writeable && !has_conflicts)));
}

bool DownloadPathReservationTracker::IsPathInUse(const FilePath& path) const {
  // Unfortunately path normalization doesn't work reliably for non-existant
  // files. So given a FilePath, we can't derive a normalized key that we can
  // use for lookups. We only expect a small number of concurrent downloads at
  // any given time, so going through all of them shouldn't be too slow.
  for (ReservationMap::const_iterator iter = reservations_.begin();
       iter != reservations_.end(); ++iter) {
    if (iter->second ==  path)
      return true;
  }
  // If the path exists in the file system, then the path is in use.
  if (file_util::PathExists(path))
    return true;

  // If the .crdownload path exists in the file system, then the path is also in
  // use. This is to avoid potential collisions for the intermediate path if
  // there is a .crdownload left around.
  if (file_util::PathExists(download_util::GetCrDownloadPath(path)))
    return true;

  return false;
}

void DownloadPathReservationTracker::Update(DownloadId download_id,
                                            const FilePath& new_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  ReservationMap::iterator iter = reservations_.find(download_id);
  if (iter != reservations_.end()) {
    iter->second = new_path;
  } else {
    // This would happen if an Update() notification was scheduled on the FILE
    // thread before ReserveInternal(), or after a Revoke() call. Neither should
    // happen.
    NOTREACHED();
  }
}

void DownloadPathReservationTracker::Revoke(DownloadId download_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(ContainsKey(reservations_, download_id));
  reservations_.erase(download_id);
}

// static
DownloadPathReservationTracker* DownloadPathReservationTracker::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static base::LazyInstance<DownloadPathReservationTracker>
      reservation_tracker = LAZY_INSTANCE_INITIALIZER;
  return reservation_tracker.Pointer();
}
