// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_DELEGATE_H_

#include "base/callback_forward.h"

#include "chrome/browser/download/download_path_reservation_tracker.h"
#include "content/public/browser/download_danger_type.h"

class ExtensionDownloadsEventRouter;

namespace base {
class FilePath;
}

namespace content {
class DownloadItem;
}

// Delegate for DownloadTargetDeterminer. The delegate isn't owned by
// DownloadTargetDeterminer and is expected to outlive it.
class DownloadTargetDeterminerDelegate {
 public:
  // Callback to be invoked after NotifyExtensions() completes. The
  // |new_virtual_path| should be set to a new path if an extension wishes to
  // override the download path. |conflict_action| should be set to the action
  // to take if a file exists at |new_virtual_path|. If |new_virtual_path| is
  // empty, then the download target will be unchanged and |conflict_action| is
  // ignored.
  typedef base::Callback<void(
      const base::FilePath& new_virtual_path,
      DownloadPathReservationTracker::FilenameConflictAction conflict_action)>
  NotifyExtensionsCallback;

  // Callback to be invoked when ReserveVirtualPath() completes. If the path
  // reservation is successful, then |successful| should be true and
  // |reserved_path| should contain the reserved path. Otherwise, |successful|
  // should be false. In the failure case, |reserved_path| is ignored.
  typedef base::Callback<void(const base::FilePath& reserved_path,
                              bool successful)> ReservedPathCallback;

  // Callback to be invoked when PromptUserForDownloadPath() completes.
  // |virtual_path|: The path chosen by the user. If the user cancels the file
  //    selection, then this parameter will be the empty path. On Chrome OS,
  //    this path may contain virtual mount points if the user chose a virtual
  //    path (e.g. Google Drive).
  typedef base::Callback<void(const base::FilePath& virtual_path)>
  FileSelectedCallback;

  // Callback to be invoked when DetermineLocalPath() completes. The argument
  // should be the determined local path. It should be non-empty on success. If
  // |virtual_path| is already a local path, then |virtual_path| should be
  // returned as-is.
  typedef base::Callback<void(const base::FilePath&)> LocalPathCallback;

  // Callback to be invoked after CheckDownloadUrl() completes. The parameter to
  // the callback should indicate the danger type of the download based on the
  // results of the URL check.
  typedef base::Callback<void(content::DownloadDangerType danger_type)>
  CheckDownloadUrlCallback;

  // Callback to be invoked after GetFileMimeType() completes. The parameter
  // should be the MIME type of the requested file. If no MIME type can be
  // determined, it should be set to the empty string.
  typedef base::Callback<void(const std::string&)> GetFileMimeTypeCallback;

  // Notifies extensions of the impending filename determination. |virtual_path|
  // is the current suggested virtual path. The |callback| should be invoked to
  // indicate whether any extensions wish to override the path.
  virtual void NotifyExtensions(content::DownloadItem* download,
                                const base::FilePath& virtual_path,
                                const NotifyExtensionsCallback& callback) = 0;

  // Reserve |virtual_path|. This is expected to check the following:
  // - Whether |virtual_path| can be written to by the user. If not, the
  //   |virtual_path| can be changed to writeable path if necessary.
  // - If |conflict_action| is UNIQUIFY then |virtual_path| should be
  //   modified so that the new path is writeable and unique. If
  //   |conflict_action| is PROMPT, then in the event of a conflict,
  //   |callback| should be invoked with |success| set to |false| in
  //   order to force a prompt. |virtual_path| may or may not be
  //   modified in the latter case.
  // - If |create_directory| is true, then the parent directory of
  //   |virtual_path| should be created if it doesn't exist.
  //
  // |callback| should be invoked on completion with the results.
  virtual void ReserveVirtualPath(
      content::DownloadItem* download,
      const base::FilePath& virtual_path,
      bool create_directory,
      DownloadPathReservationTracker::FilenameConflictAction conflict_action,
      const ReservedPathCallback& callback) = 0;

  // Display a prompt to the user requesting that a download target be chosen.
  // Should invoke |callback| upon completion.
  virtual void PromptUserForDownloadPath(
      content::DownloadItem* download,
      const base::FilePath& virtual_path,
      const FileSelectedCallback& callback) = 0;

  // If |virtual_path| is not a local path, should return a possibly temporary
  // local path to use for storing the downloaded file. If |virtual_path| is
  // already local, then it should return the same path. |callback| should be
  // invoked to return the path.
  virtual void DetermineLocalPath(content::DownloadItem* download,
                                  const base::FilePath& virtual_path,
                                  const LocalPathCallback& callback) = 0;

  // Check whether the download URL is malicious and invoke |callback| with a
  // suggested danger type for the download.
  virtual void CheckDownloadUrl(content::DownloadItem* download,
                                const base::FilePath& virtual_path,
                                const CheckDownloadUrlCallback& callback) = 0;

  // Get the MIME type for the given file.
  virtual void GetFileMimeType(const base::FilePath& path,
                               const GetFileMimeTypeCallback& callback) = 0;
 protected:
  virtual ~DownloadTargetDeterminerDelegate();
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_DELEGATE_H_
