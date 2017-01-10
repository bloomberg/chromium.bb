// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_path_reservation_tracker.h"

#include <stddef.h>

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/third_party/icu/icu_utf.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"

using content::BrowserThread;
using content::DownloadItem;

namespace {

typedef DownloadItem* ReservationKey;
typedef std::map<ReservationKey, base::FilePath> ReservationMap;

// The lower bound for file name truncation. If the truncation results in a name
// shorter than this limit, we give up automatic truncation and prompt the user.
static const size_t kTruncatedNameLengthLowerbound = 5;

// The length of the suffix string we append for an intermediate file name.
// In the file name truncation, we keep the margin to append the suffix.
// TODO(kinaba): remove the margin. The user should be able to set maximum
// possible filename.
static const size_t kIntermediateNameSuffixLength = sizeof(".crdownload") - 1;

// Map of download path reservations. Each reserved path is associated with a
// ReservationKey=DownloadItem*. This object is destroyed in |Revoke()| when
// there are no more reservations.
//
// It is not an error, although undesirable, to have multiple DownloadItem*s
// that are mapped to the same path. This can happen if a reservation is created
// that is supposed to overwrite an existing reservation.
ReservationMap* g_reservation_map = NULL;

// Observes a DownloadItem for changes to its target path and state. Updates or
// revokes associated download path reservations as necessary. Created, invoked
// and destroyed on the UI thread.
class DownloadItemObserver : public DownloadItem::Observer,
                             public base::SupportsUserData::Data {
 public:
  explicit DownloadItemObserver(DownloadItem* download_item);

 private:
  ~DownloadItemObserver() override;

  // DownloadItem::Observer
  void OnDownloadUpdated(DownloadItem* download) override;
  void OnDownloadDestroyed(DownloadItem* download) override;

  DownloadItem* download_item_;

  // Last known target path for the download.
  base::FilePath last_target_path_;

  static const int kUserDataKey;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemObserver);
};

// Returns true if the given path is in use by a path reservation.
bool IsPathReserved(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  // No reservation map => no reservations.
  if (g_reservation_map == NULL)
    return false;

  // We only expect a small number of concurrent downloads at any given time, so
  // going through all of them shouldn't be too slow.
  for (ReservationMap::const_iterator iter = g_reservation_map->begin();
       iter != g_reservation_map->end(); ++iter) {
    if (base::FilePath::CompareEqualIgnoreCase(iter->second.value(),
                                               path.value()))
      return true;
  }
  return false;
}

// Returns true if the given path is in use by any path reservation or the
// file system. Called on the FILE thread.
bool IsPathInUse(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  // If there is a reservation, then the path is in use.
  if (IsPathReserved(path))
    return true;

  // If the path exists in the file system, then the path is in use.
  if (base::PathExists(path))
    return true;

  return false;
}

// Truncates path->BaseName() to make path->BaseName().value().size() <= limit.
// - It keeps the extension as is. Only truncates the body part.
// - It secures the base filename length to be more than or equals to
//   kTruncatedNameLengthLowerbound.
// If it was unable to shorten the name, returns false.
bool TruncateFileName(base::FilePath* path, size_t limit) {
  base::FilePath basename(path->BaseName());
  // It is already short enough.
  if (basename.value().size() <= limit)
    return true;

  base::FilePath dir(path->DirName());
  base::FilePath::StringType ext(basename.Extension());
  base::FilePath::StringType name(basename.RemoveExtension().value());

  // Impossible to satisfy the limit.
  if (limit < kTruncatedNameLengthLowerbound + ext.size())
    return false;
  limit -= ext.size();

  // Encoding specific truncation logic.
  base::FilePath::StringType truncated;
#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
  // UTF-8.
  base::TruncateUTF8ToByteSize(name, limit, &truncated);
#elif defined(OS_WIN)
  // UTF-16.
  DCHECK(name.size() > limit);
  truncated = name.substr(0, CBU16_IS_TRAIL(name[limit]) ? limit - 1 : limit);
#else
  // We cannot generally assume that the file name encoding is in UTF-8 (see
  // the comment for FilePath::AsUTF8Unsafe), hence no safe way to truncate.
#endif

  if (truncated.size() < kTruncatedNameLengthLowerbound)
    return false;
  *path = dir.Append(truncated + ext);
  return true;
}

// Called on the FILE thread to reserve a download path. This method:
// - Creates directory |default_download_path| if it doesn't exist.
// - Verifies that the parent directory of |suggested_path| exists and is
//   writeable.
// - Truncates the suggested name if it exceeds the filesystem's limit.
// - Uniquifies |suggested_path| if |should_uniquify_path| is true.
// - Returns true if |reserved_path| has been successfully verified.
bool CreateReservation(
    ReservationKey key,
    const base::FilePath& suggested_path,
    const base::FilePath& default_download_path,
    bool create_directory,
    DownloadPathReservationTracker::FilenameConflictAction conflict_action,
    base::FilePath* reserved_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(suggested_path.IsAbsolute());

  // Create a reservation map if one doesn't exist. It will be automatically
  // deleted when all the reservations are revoked.
  if (g_reservation_map == NULL)
    g_reservation_map = new ReservationMap;

  // Erase the reservation if it already exists. This can happen during
  // automatic resumption where a new target determination request may be issued
  // for a DownloadItem without an intervening transition to INTERRUPTED.
  //
  // Revoking and re-acquiring the reservation forces us to re-verify the claims
  // we are making about the path.
  g_reservation_map->erase(key);

  base::FilePath target_path(suggested_path.NormalizePathSeparators());
  base::FilePath target_dir = target_path.DirName();
  base::FilePath filename = target_path.BaseName();
  bool is_path_writeable = true;
  bool has_conflicts = false;
  bool name_too_long = false;

  // Create target_dir if necessary and appropriate. target_dir may be the last
  // directory that the user selected in a FilePicker; if that directory has
  // since been removed, do NOT automatically re-create it. Only automatically
  // create the directory if it is the default Downloads directory or if the
  // caller explicitly requested automatic directory creation.
  if (!base::DirectoryExists(target_dir) &&
      (create_directory ||
       (!default_download_path.empty() &&
        (default_download_path == target_dir)))) {
    base::CreateDirectory(target_dir);
  }

  // Check writability of the suggested path. If we can't write to it, default
  // to the user's "My Documents" directory. We'll prompt them in this case.
  if (!base::PathIsWritable(target_dir)) {
    DVLOG(1) << "Unable to write to directory \"" << target_dir.value() << "\"";
#if defined(OS_ANDROID)
    // On Android, DIR_USER_DOCUMENTS is in reality a subdirectory
    // of DIR_ANDROID_APP_DATA which isn't accessible by other apps.
    reserved_path->clear();
    (*g_reservation_map)[key] = *reserved_path;
    return false;
#else
    is_path_writeable = false;
    PathService::Get(chrome::DIR_USER_DOCUMENTS, &target_dir);
    target_path = target_dir.Append(filename);
#endif  // defined(OS_ANDROID)
  }

  if (is_path_writeable) {
    // Check the limit of file name length if it could be obtained. When the
    // suggested name exceeds the limit, truncate or prompt the user.
    int max_length = base::GetMaximumPathComponentLength(target_dir);
    if (max_length != -1) {
      int limit = max_length - kIntermediateNameSuffixLength;
      if (limit <= 0 || !TruncateFileName(&target_path, limit))
        name_too_long = true;
    }

    // Uniquify the name, if it already exists.
    if (!name_too_long && IsPathInUse(target_path)) {
      has_conflicts = true;
      if (conflict_action == DownloadPathReservationTracker::OVERWRITE) {
        has_conflicts = false;
      }
      // If ...PROMPT, then |has_conflicts| will remain true, |verified| will be
      // false, and CDMD will prompt.
      if (conflict_action == DownloadPathReservationTracker::UNIQUIFY) {
        for (int uniquifier = 1;
            uniquifier <= DownloadPathReservationTracker::kMaxUniqueFiles;
            ++uniquifier) {
          // Append uniquifier.
          std::string suffix(base::StringPrintf(" (%d)", uniquifier));
          base::FilePath path_to_check(target_path);
          // If the name length limit is available (max_length != -1), and the
          // the current name exceeds the limit, truncate.
          if (max_length != -1) {
            int limit =
                max_length - kIntermediateNameSuffixLength - suffix.size();
            // If truncation failed, give up uniquification.
            if (limit <= 0 || !TruncateFileName(&path_to_check, limit))
              break;
          }
          path_to_check = path_to_check.InsertBeforeExtensionASCII(suffix);

          if (!IsPathInUse(path_to_check)) {
            target_path = path_to_check;
            has_conflicts = false;
            break;
          }
        }
      }
    }
  }

  (*g_reservation_map)[key] = target_path;
  bool verified = (is_path_writeable && !has_conflicts && !name_too_long);
  *reserved_path = target_path;
  return verified;
}

// Called on the FILE thread to update the path of the reservation associated
// with |key| to |new_path|.
void UpdateReservation(ReservationKey key, const base::FilePath& new_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(g_reservation_map != NULL);
  ReservationMap::iterator iter = g_reservation_map->find(key);
  if (iter != g_reservation_map->end()) {
    iter->second = new_path;
  } else {
    // This would happen if an UpdateReservation() notification was scheduled on
    // the FILE thread before ReserveInternal(), or after a Revoke()
    // call. Neither should happen.
    NOTREACHED();
  }
}

// Called on the FILE thread to remove the path reservation associated with
// |key|.
void RevokeReservation(ReservationKey key) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(g_reservation_map != NULL);
  DCHECK(base::ContainsKey(*g_reservation_map, key));
  g_reservation_map->erase(key);
  if (g_reservation_map->size() == 0) {
    // No more reservations. Delete map.
    delete g_reservation_map;
    g_reservation_map = NULL;
  }
}

void RunGetReservedPathCallback(
    const DownloadPathReservationTracker::ReservedPathCallback& callback,
    const base::FilePath* reserved_path,
    bool verified) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback.Run(*reserved_path, verified);
}

DownloadItemObserver::DownloadItemObserver(DownloadItem* download_item)
    : download_item_(download_item),
      last_target_path_(download_item->GetTargetFilePath()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  download_item_->AddObserver(this);
  download_item_->SetUserData(&kUserDataKey, this);
}

DownloadItemObserver::~DownloadItemObserver() {
  download_item_->RemoveObserver(this);
  // DownloadItemObserver is owned by DownloadItem. It should only be getting
  // destroyed because it's being removed from the UserData pool. No need to
  // call DownloadItem::RemoveUserData().
}

void DownloadItemObserver::OnDownloadUpdated(DownloadItem* download) {
  switch (download->GetState()) {
    case DownloadItem::IN_PROGRESS: {
      // Update the reservation.
      base::FilePath new_target_path = download->GetTargetFilePath();
      if (new_target_path != last_target_path_) {
        BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
            &UpdateReservation, download, new_target_path));
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

    case DownloadItem::INTERRUPTED:
      // The download filename will need to be re-generated when the download is
      // restarted. Holding on to the reservation now would prevent the name
      // from being used for a subsequent retry attempt.

      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
          &RevokeReservation, download));
      download->RemoveUserData(&kUserDataKey);
      break;

    case DownloadItem::MAX_DOWNLOAD_STATE:
      // Compiler appeasement.
      NOTREACHED();
  }
}

void DownloadItemObserver::OnDownloadDestroyed(DownloadItem* download) {
  // Items should be COMPLETE/INTERRUPTED/CANCELLED before being destroyed.
  NOTREACHED();
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
      &RevokeReservation, download));
}

// static
const int DownloadItemObserver::kUserDataKey = 0;

}  // namespace

// static
void DownloadPathReservationTracker::GetReservedPath(
    DownloadItem* download_item,
    const base::FilePath& target_path,
    const base::FilePath& default_path,
    bool create_directory,
    FilenameConflictAction conflict_action,
    const ReservedPathCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Attach an observer to the download item so that we know when the target
  // path changes and/or the download is no longer active.
  new DownloadItemObserver(download_item);
  // DownloadItemObserver deletes itself.

  base::FilePath* reserved_path = new base::FilePath;
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CreateReservation,
                 download_item,
                 target_path,
                 default_path,
                 create_directory,
                 conflict_action,
                 reserved_path),
      base::Bind(&RunGetReservedPathCallback,
                 callback,
                 base::Owned(reserved_path)));
}

// static
bool DownloadPathReservationTracker::IsPathInUseForTesting(
    const base::FilePath& path) {
  return IsPathInUse(path);
}
