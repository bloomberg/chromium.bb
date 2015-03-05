// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREF_NAMES_H_
#define IOS_CHROME_BROWSER_PREF_NAMES_H_

namespace ios {
namespace prefs {

// Preferences in ios::prefs:: are temporary shared with desktop Chrome.
// Non-shared preferences should be in the prefs:: namespace (no ios::).
extern const char kAcceptLanguages[];

}  // namespace prefs
}  // namespace ios

namespace prefs {

extern const char kIosBookmarkFolderDefault[];
extern const char kIosBookmarkPromoAlreadySeen[];
extern const char kPaymentsPreferredUserId[];

}  // namespace prefs

#endif  // IOS_CHROME_BROWSER_PREF_NAMES_H_
