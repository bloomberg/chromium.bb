// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_WIFI_CREDENTIALS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_WIFI_CREDENTIALS_HELPER_H_

// Functions needed by multiple wifi_credentials integration
// tests. This module is platfrom-agnostic, and calls out to
// platform-specific code as needed.
namespace wifi_credentials_helper {

// Checks if the verifier has any items in it. Returns true iff the
// verifier has no items.
bool VerifierIsEmpty();

// Compares the BrowserContext for |profile_index| with the
// verifier. Returns true iff their WiFi credentials match.
bool ProfileMatchesVerifier(int profile_index);

// Returns true iff all BrowserContexts match with the verifier.
bool AllProfilesMatch();

}  // namespace wifi_credentials_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_WIFI_CREDENTIALS_HELPER_H_
