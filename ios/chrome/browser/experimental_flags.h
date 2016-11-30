// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_
#define IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_

#include <string>

// This file can be empty. Its purpose is to contain the relatively short lived
// declarations required for experimental flags.

namespace experimental_flags {

// Whether background crash report upload should generate a local notification.
bool IsAlertOnBackgroundUploadEnabled();

// Whether the lru snapshot cache experiment is enabled.
bool IsLRUSnapshotCacheEnabled();

// Whether viewing and copying passwords is enabled.
bool IsViewCopyPasswordsEnabled();

// Whether password generation is enabled.
bool IsPasswordGenerationEnabled();

// Whether password generation fields are determined using local heuristics
// only.
bool UseOnlyLocalHeuristicsForPasswordGeneration();

// Whether the Tab Switcher is enabled for iPad or not.
bool IsTabSwitcherEnabled();

// Whether the reading list is enabled.
bool IsReadingListEnabled();

// Whether the All Bookmarks view is visible in bookmarks.
bool IsAllBookmarksEnabled();

// Whether the Physical Web feature is enabled.
bool IsPhysicalWebEnabled();

// Whether the QR Code Reader is enabled.
bool IsQRCodeReaderEnabled();

// Whether the Clear Browsing Data counters and time selection UI is enabled.
bool IsNewClearBrowsingDataUIEnabled();

// Whether the Payment Request API is enabled or not.
bool IsPaymentRequestEnabled();

// Whether launching actions from Spotlight is enabled.
bool IsSpotlightActionsEnabled();

// Whether the iOS MDM integration is enabled.
bool IsMDMIntegrationEnabled();

// Whether the back-forward navigation uses pending index.
bool IsPendingIndexNavigationEnabled();

// Whether "Save Image" should be renamed as "Download Image".
bool IsDownloadRenamingEnabled();
}  // namespace experimental_flags

#endif  // IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_
