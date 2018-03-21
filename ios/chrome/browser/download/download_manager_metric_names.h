// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_METRIC_NAMES_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_METRIC_NAMES_H_

// MobileDownloadFileUIInstallGoogleDrive UMA action. Logged when Google Drive
// app is installed after presenting Store Kit dialog from the Download Manager.
extern const char kDownloadManagerGoogleDriveInstalled[];

// Values of the UMA Download.IOSDownloadedFileAction histogram. This enum is
// append only.
enum class DownloadedFileAction {
  // Downloaded file was uploaded to Google Drive.
  OpenedInDrive = 0,
  // Downloaded file was open in the app other than Google Drive.
  OpenedInOtherApp = 1,
  // Downloaded file was discarded (the user closed the app, tab, or download
  // manager UI).
  NoAction = 2,
  Count,
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_METRIC_NAMES_H_
