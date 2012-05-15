// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_KEYCHAIN_REAUTHORIZE_H_
#define CHROME_BROWSER_MAC_KEYCHAIN_REAUTHORIZE_H_
#pragma once

#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif

namespace chrome {
namespace browser {
namespace mac {

// Reauthorizes all Keychain items that can be found in a standard Keychain
// search, as long as they are accessible and can be decrypted. This operates
// by scanning the requirement strings for each application in each ACL in
// each accessible Keychain item. If any requirement string matches a list of
// strings to perform reauthorization for, the matching application in the ACL
// will be replaced with this application, using this application's designated
// requirement as the requirement string. Keychain items that are reauthorized
// are made effective by deleting the original item and storing the new one
// with its revised access policy in the Keychain. This circuitous method is
// used because applications don't generally have permission to modify access
// control policies on existing Keychain items (even when they are able to
// decrypt those items), but any application can remove a Keychain item.
void KeychainReauthorize();

// Calls KeychainReauthorize, but only if it's determined that it's necessary.
// pref_key is looked up in the system's standard user defaults (preferences)
// and if its integer value is less than max_tries, KeychainReauthorize is
// attempted. Before the attempt, the preference is incremented, allowing a
// finite number of incomplete attempts at performing the KeychainReauthorize
// operation. When the step completes successfully, the preference is set to
// max_tries to prevent further attempts, and the preference name with the
// word "Success" appended is also stored with a boolean value of YES,
// disambiguating between the cases where the step completed successfully and
// the step completed unsuccessfully while reaching the maximum number of
// tries.
//
// The system's standard user defaults for the application are used
// (~/Library/Preferences/com.google.Chrome.plist,
// com.google.Chrome.canary.plist, etc.) instead of Chrome preferences because
// Keychain access is tied more closely to the bundle identifier and signed
// product than it is to any specific profile (--user-data-dir).
void KeychainReauthorizeIfNeeded(NSString* pref_key, int max_tries);

}  // namespace mac
}  // namespace browser
}  // namespace chrome

#endif  // CHROME_BROWSER_MAC_KEYCHAIN_REAUTHORIZE_H_
