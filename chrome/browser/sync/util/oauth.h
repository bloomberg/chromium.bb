// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_OAUTH_H_
#define CHROME_BROWSER_SYNC_UTIL_OAUTH_H_
#pragma once

namespace browser_sync {

// TODO(rickcam): Bug(92948): Remove IsUsingOAuth after ClientLogin is gone
bool IsUsingOAuth();

// TODO(rickcam): Bug(92948): Remove SyncServiceName post-ClientLogin
const char* SyncServiceName();

// TODO(rickcam): Bug(92948): Remove SetIsUsingOAuthForTest post-ClientLogin
void SetIsUsingOAuthForTest(bool is_using_oauth);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_OAUTH_H_
