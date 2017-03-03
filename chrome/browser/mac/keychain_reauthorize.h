// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_KEYCHAIN_REAUTHORIZE_H_
#define CHROME_BROWSER_MAC_KEYCHAIN_REAUTHORIZE_H_

#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif

namespace chrome {

// Reauthorizes the keychain entry, but only if it's determined that it's
// necessary. pref_key is looked up in the system's standard user defaults
// (preferences) and it is associated with both the number of previous attempts,
// and whether a previous attempt succeeded. Only if a previous attempt did not
// succeed, and the number of previous tries is less than max_tries, is
// reauthorization attempted. Before the attempt, the preference is
// incremented, allowing a finite number of incomplete attempts at performing
// the operation.

// The system's standard user defaults for the application are used
// (~/Library/Preferences/com.google.Chrome.plist,
// com.google.Chrome.canary.plist, etc.) instead of Chrome preferences because
// Keychain access is tied more closely to the bundle identifier and signed
// product than it is to any specific profile (--user-data-dir).
void KeychainReauthorizeIfNeeded(NSString* pref_key, int max_tries);

}  // namespace chrome

#endif  // CHROME_BROWSER_MAC_KEYCHAIN_REAUTHORIZE_H_
