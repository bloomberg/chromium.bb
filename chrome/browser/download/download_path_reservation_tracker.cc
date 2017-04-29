// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_path_reservation_tracker.h"

#include <stddef.h>

#include <map>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
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
#include "net/base/filename_util.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::DownloadItem;

namespace {

typedef DownloadItem* ReservationKey;
typedef std::map<ReservationKey, base::FilePath> ReservationMap;

// The lower bound for file name truncation. If the truncation results in a name
// shorter than this limit, we give up automatic truncation and prompt the user.
const size_t kTruncatedNameLengthLowerbound = 5;

// The length of the suffix string we append for an intermediate file name.
// In the file name truncation, we keep the margin to append the suffix.
// TODO(kinaba): remove the margin. The user should be able to set maximum
// possible filename.
const size_t kIntermediateNameSuffixLength = sizeof(".crdownload") - 1;

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
  ~DownloadItemObserver() override;

 private:
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

// Create a unique filename by appending a uniquifier. Modifies |path| in place
// if successful and returns true. Otherwise |path| is left unmodified and
// returns false.
bool CreateUniqueFilename(int max_path_component_length, base::FilePath* path) {
  for (int uniquifier = 1;
       uniquifier <= DownloadPathReservationTracker::kMaxUniqueFiles;
       ++uniquifier) {
    // Append uniquifier.
    std::string suffix(base::StringPrintf(" (%d)", uniquifier));
    base::FilePath path_to_check(*path);
    // If the name length limit is available (max_length != -1), and the
    // the current name exceeds the limit, truncate.
    if (max_path_component_length != -1) {
      int limit = max_path_component_length - kIntermediateNameSuffixLength -
                  suffix.size();
      // If truncation failed, give up uniquification.
      if (limit <= 0 || !TruncateFileName(&path_to_check, limit))
        break;
    }
    path_to_check = path_to_check.InsertBeforeExtensionASCII(suffix);

    if (!IsPathInUse(path_to_check)) {
      *path = path_to_check;
      return true;
    }
  }
  return false;
}

struct CreateReservationInfo {
  ReservationKey key;
  base::FilePath source_path;
  base::FilePath suggested_path;
  base::FilePath default_download_path;
  bool create_target_directory;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action;
  DownloadPathReservationTracker::ReservedPathCallback completion_callback;
};

// Verify that |target_path| can be written to and also resolve any conflicts if
// necessary by uniquifying the filename.
PathValidationResult ValidatePathAndResolveConflicts(
    const CreateReservationInfo& info,
    base::FilePath* target_path) {
  // Check writability of the suggested path. If we can't write to it, default
  // to the user's Documents directory. We'll prompt them in this case. No
  // further amendments are made to the filename since the user is going to be
  // prompted.
  if (!base::PathIsWritable(target_path->DirName())) {
    DVLOG(1) << "Unable to write to path \"" << target_path->value() << "\"";
    base::FilePath target_dir;
    PathService::Get(chrome::DIR_USER_DOCUMENTS, &target_dir);
    *target_path = target_dir.Append(target_path->BaseName());
    return PathValidationResult::PATH_NOT_WRITABLE;
  }

  int max_path_component_length =
      base::GetMaximumPathComponentLength(target_path->DirName());
  // Check the limit of file name length if it could be obtained. When the
  // suggested name exceeds the limit, truncate or prompt the user.
  if (max_path_component_length != -1) {
    int limit = max_path_component_length - kIntermediateNameSuffixLength;
    if (limit <= 0 || !TruncateFileName(target_path, limit))
      return PathValidationResult::NAME_TOO_LONG;
  }

  // Disallow downloading a file onto itself. Assume that downloading a file
  // onto another file that differs only by case is not enough of a legitimate
  // edge case to justify determining the case sensitivity of the underlying
  // filesystem.
  if (*target_path == info.source_path)
    return PathValidationResult::SAME_AS_SOURCE;

  if (!IsPathInUse(*target_path))
    return PathValidationResult::SUCCESS;

  switch (info.conflict_action) {
    case DownloadPathReservationTracker::UNIQUIFY:
      return CreateUniqueFilename(max_path_component_length, target_path)
                 ? PathValidationResult::SUCCESS
                 : PathValidationResult::CONFLICT;

    case DownloadPathReservationTracker::OVERWRITE:
      return PathValidationResult::SUCCESS;

    case DownloadPathReservationTracker::PROMPT:
      return PathValidationResult::CONFLICT;
  }
  NOTREACHED();
  return PathValidationResult::SUCCESS;
}

// Called on the FILE thread to reserve a download path. This method:
// - Creates directory |default_download_path| if it doesn't exist.
// - Verifies that the parent directory of |suggested_path| exists and is
//   writeable.
// - Truncates the suggested name if it exceeds the filesystem's limit.
// - Uniquifies |suggested_path| if |should_uniquify_path| is true.
// - Schedules |callback| on the UI thread with the reserved path and a flag
//   indicating whether the returned path has been successfully verified.
// - Returns the result of creating the path reservation.
PathValidationResult CreateReservation(const CreateReservationInfo& info,
                                       base::FilePath* reserved_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(info.suggested_path.IsAbsolute());

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
  g_reservation_map->erase(info.key);

  base::FilePath target_path(info.suggested_path.NormalizePathSeparators());
  base::FilePath target_dir = target_path.DirName();
  base::FilePath filename = target_path.BaseName();

  // Create target_dir if necessary and appropriate. target_dir may be the last
  // directory that the user selected in a FilePicker; if that directory has
  // since been removed, do NOT automatically re-create it. Only automatically
  // create the directory if it is the default Downloads directory or if the
  // caller explicitly requested automatic directory creation.
  if (!base::DirectoryExists(target_dir) &&
      (info.create_target_directory ||
       (!info.default_download_path.empty() &&
        (info.default_download_path == target_dir)))) {
    base::CreateDirectory(target_dir);
  }

  PathValidationResult result =
      ValidatePathAndResolveConflicts(info, &target_path);
  (*g_reservation_map)[info.key] = target_path;
  *reserved_path = target_path;
  return result;
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
    PathValidationResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback.Run(result, *reserved_path);
}

DownloadItemObserver::DownloadItemObserver(DownloadItem* download_item)
    : download_item_(download_item),
      last_target_path_(download_item->GetTargetFilePath()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  download_item_->AddObserver(this);
  download_item_->SetUserData(&kUserDataKey, base::WrapUnique(this));
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
        BrowserThread::PostTask(
            BrowserThread::FILE, FROM_HERE,
            base::BindOnce(&UpdateReservation, download, new_target_path));
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

      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                              base::BindOnce(&RevokeReservation, download));
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
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::BindOnce(&RevokeReservation, download));
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
  base::FilePath source_path;
  if (download_item->GetURL().SchemeIsFile())
    net::FileURLToFilePath(download_item->GetURL(), &source_path);
  CreateReservationInfo info = {static_cast<ReservationKey>(download_item),
                                source_path,
                                target_path,
                                default_path,
                                create_directory,
                                conflict_action,
                                callback};

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateReservation, info, reserved_path),
      base::Bind(&RunGetReservedPathCallback, callback,
                 base::Owned(reserved_path)));
}

// static
bool DownloadPathReservationTracker::IsPathInUseForTesting(
    const base::FilePath& path) {
  return IsPathInUse(path);
}
