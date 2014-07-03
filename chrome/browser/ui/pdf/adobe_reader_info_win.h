// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_ADOBE_READER_INFO_WIN_H_
#define CHROME_BROWSER_UI_PDF_ADOBE_READER_INFO_WIN_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "content/public/common/webplugininfo.h"

class Profile;

struct AdobeReaderPluginInfo {
  bool is_installed;
  bool is_enabled;    // Only valid in the context of a given Profile.
  bool is_secure;     // Whether the plugin is up to date.
  content::WebPluginInfo plugin_info;
};

typedef base::Callback<void(const AdobeReaderPluginInfo&)>
    GetAdobeReaderPluginInfoCallback;

// Fetches information about the Adobe Reader plugin asynchronously.
// If |profile| is NULL, then the plugin's enable status cannot be
// determined.
void GetAdobeReaderPluginInfoAsync(
    Profile* profile,
    const GetAdobeReaderPluginInfoCallback& callback);

// Fetches information about the Adobe Reader plugin synchronously.
// Returns true if the plugin info is not stale.
// If |profile| is NULL, then the plugin's enable status cannot be
// determined.
bool GetAdobeReaderPluginInfo(Profile* profile,
                              AdobeReaderPluginInfo* reader_info);

// Returns true if Adobe Reader or Adobe Acrobat is the default viewer for the
// .pdf extension.
bool IsAdobeReaderDefaultPDFViewer();

// If Adobe Reader or Adobe Acrobat is program associated with the .pdf viewer,
// return true if the executable is up to date.
// If Reader/Acrobat is not the default .pdf handler, return false.
// This function does blocking I/O, since it needs to read from the disk.
bool IsAdobeReaderUpToDate();

#endif  // CHROME_BROWSER_UI_PDF_ADOBE_READER_INFO_WIN_H_
