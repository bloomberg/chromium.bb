// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utility functions for Mac OS X.

#ifndef CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_UTIL_MAC_H_
#define CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_UTIL_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>

class FilePath;

namespace download_util {

void AddFileToPasteboard(NSPasteboard* pasteboard, const FilePath& path);

// Notify the system that a download completed. This will cause the download
// folder in the dock to bounce.
void NotifySystemOfDownloadComplete(const FilePath& path);

}  // namespace download_util

#endif  // CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_UTIL_MAC_H_
