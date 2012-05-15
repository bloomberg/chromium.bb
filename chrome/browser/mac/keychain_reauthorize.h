// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_KEYCHAIN_REAUTHORIZE_H_
#define CHROME_BROWSER_MAC_KEYCHAIN_REAUTHORIZE_H_
#pragma once

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

}  // namespace mac
}  // namespace browser
}  // namespace chrome

#endif  // CHROME_BROWSER_MAC_KEYCHAIN_REAUTHORIZE_H_
