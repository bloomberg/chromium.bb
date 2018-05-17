// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_LOCATION_DIALOG_TYPE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_LOCATION_DIALOG_TYPE_H_

// The type of download location dialog that should by shown by Android.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.download
enum class DownloadLocationDialogType {
  NO_DIALOG,           // No dialog.
  DEFAULT,             // Dialog without any error states.
  LOCATION_FULL,       // Error dialog, default location is full.
  LOCATION_NOT_FOUND,  // Error dialog, default location is not found.
  NAME_CONFLICT,       // Error dialog, there is already a file with that name.
  NAME_TOO_LONG,       // Error dialog, the file name is too long.
};

// Result of download location dialog.
enum class DownloadLocationDialogResult {
  USER_CONFIRMED = 0,  // User confirmed a file path.
  USER_CANCELED,       // User canceled file path selection.
  DUPLICATE_DIALOG,    // Dialog is already showing.
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_LOCATION_DIALOG_TYPE_H_
