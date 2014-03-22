// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_INTEGRATION_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_INTEGRATION_TEST_UTIL_H_

class ProfileSyncService;

namespace sync_integration_test_util {

// Wait until the provided |service| is blocked waiting for a passphrase.
bool AwaitPassphraseRequired(ProfileSyncService* service);

// Wait until the provided |service| has accepted the new passphrase.
bool AwaitPassphraseAccepted(ProfileSyncService* service);

// Wait until the |service| is fully synced.
// This can be a bit flaky.  See UpdatedProgressMarkerChecker for details.
bool AwaitCommitActivityCompletion(ProfileSyncService* service);

}  // namespace sync_integration_test_util

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_INTEGRATION_TEST_UTIL_H_
