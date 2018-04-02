// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_METRIC_NAMES_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_METRIC_NAMES_H_

// Values of the UMA Download.IOSDownloadedFileAction histogram. This enum is
// append only.
enum class DownloadedFileAction {
  // Downloaded file was uploaded to Google Drive.
  OpenedInDrive = 0,
  // Downloaded file was open in the app other than Google Drive.
  OpenedInOtherApp = 1,
  // Downloaded file was discarded (the user closed the app, tab, or download
  // manager UI) or opened via Extension (Chrome is not notified if the download
  // was open in the extension).
  NoActionOrOpenedViaExtension = 2,
  Count,
};

// Values of the UMA Download.IOSDownloadFileResult histogram. This action is
// reported only for started downloads.
enum class DownloadFileResult {
  // Download has successfully completed.
  Completed = 0,
  // In progress download was cancelled by the user.
  Cancelled = 1,
  // Download has completed with error.
  Failure = 2,
  // In progress download did no finish because the tab was closed or user has
  // quit the app.
  Other = 3,
  // The user closed Download Manager UI without starting the download.
  NotStarted = 4,
  Count
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_METRIC_NAMES_H_
