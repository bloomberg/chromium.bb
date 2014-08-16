// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_SYNC_METRICS_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_SYNC_METRICS_H_

#include <string>

class Profile;

namespace password_manager_sync_metrics {

// Returns the sync username for |profile|. Returns an empty string if the
// |profile| isn't syncing. This function tries to return an empty string if
// the user isn't syncing passwords, but it is not always possibly to determine
// since this code can be called during sync setup (http://crbug.com/393626).
std::string GetSyncUsername(Profile* profile);

// Returns true if |username| and |origin| correspond to the account which is
// syncing. Will return false if |profile| is not syncing.
bool IsSyncAccountCredential(Profile* profile,
                             const std::string& username,
                             const std::string& origin);

}  // namespace password_manager_sync_metrics

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_SYNC_METRICS_H_
