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

// Whether the new bookmark collection experience is enabled.
bool IsBookmarkCollectionEnabled();

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

}  // namespace experimental_flags

#endif  // IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_
