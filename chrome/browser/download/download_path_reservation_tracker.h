// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PATH_RESERVATION_TRACKER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PATH_RESERVATION_TRACKER_H_

#include "base/callback_forward.h"

namespace base {
class FilePath;
}

namespace content {
class DownloadItem;
}

enum class PathValidationResult {
  SUCCESS,
  PATH_NOT_WRITABLE,
  NAME_TOO_LONG,
  CONFLICT,
  SAME_AS_SOURCE
};

// Chrome attempts to uniquify filenames that are assigned to downloads in order
// to avoid overwriting files that already exist on the file system. Downloads
// that are considered potentially dangerous use random intermediate filenames.
// Therefore only considering files that exist on the filesystem is
// insufficient. This class tracks files that are assigned to active downloads
// so that uniquification can take those into account as well.
class DownloadPathReservationTracker {
 public:
  // Callback used with |GetReservedPath|. |target_path| specifies the target
  // path for the download. If |result| is SUCCESS then:
  // - |requested_target_path| (passed into GetReservedPath()) was writeable.
  // - |target_path| was verified as being unique if uniqueness was
  //   required.
  //
  // If |requested_target_path| was not writeable, then the parent directory of
  // |target_path| may be different from that of |requested_target_path|.
  using ReservedPathCallback =
      base::Callback<void(PathValidationResult result,
                          const base::FilePath& target_path)>;

  // The largest index for the uniquification suffix that we will try while
  // attempting to come up with a unique path.
  static const int kMaxUniqueFiles = 100;

  enum FilenameConflictAction {
    UNIQUIFY,
    OVERWRITE,
    PROMPT,
  };

  // When a path needs to be assigned to a download, this method is called on
  // the UI thread along with a reference to the download item that will
  // eventually receive the reserved path. This method creates a path
  // reservation that will live until |download_item| is interrupted, cancelled,
  // completes or is removed. This method will not modify |download_item|.
  //
  // The process of issuing a reservation happens on the FILE thread, and
  // involves:
  //
  // - Creating |requested_target_path.DirName()| if it doesn't already exist
  //   and either |create_directory| or |requested_target_path.DirName() ==
  //   default_download_path|.
  //
  // - Verifying that |requested_target_path| is writeable. If not, the user's
  //   documents folder is used instead.
  //
  // - Uniquifying |requested_target_path| by suffixing the filename with a
  //   uniquifier (e.g. "foo.txt" -> "foo (1).txt") in order to avoid conflicts
  //   with files that already exist on the file system or other download path
  //   reservations. Uniquifying is only done if |conflict_action| is UNIQUIFY.
  //
  // - Posting a task back to the UI thread to invoke |completion_callback| with
  //   the reserved path and a bool indicating whether the returned path was
  //   verified as being writeable and unique.
  //
  // In addition, if the target path of |download_item| is changed to a path
  // other than the reserved path, then the reservation will be updated to
  // match. Such changes can happen if a "Save As" dialog was displayed and the
  // user chose a different path. The new target path is not checked against
  // active paths to enforce uniqueness. It is only used for uniquifying new
  // reservations.
  //
  // Once |completion_callback| is invoked, it is the caller's responsibility to
  // handle cases where the target path could not be verified and set the target
  // path of the |download_item| appropriately.
  //
  // The current implementation doesn't look at symlinks/mount points. E.g.: It
  // considers 'foo/bar/x.pdf' and 'foo/baz/x.pdf' to be two different paths,
  // even though 'bar' might be a symlink to 'baz'.
  static void GetReservedPath(
      content::DownloadItem* download_item,
      const base::FilePath& requested_target_path,
      const base::FilePath& default_download_path,
      bool create_directory,
      FilenameConflictAction conflict_action,
      const ReservedPathCallback& callback);

  // Returns true if |path| is in use by an existing path reservation. Should
  // only be called on the FILE thread. Currently only used by tests.
  static bool IsPathInUseForTesting(const base::FilePath& path);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PATH_RESERVATION_TRACKER_H_
