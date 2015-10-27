// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_UNVERIFIED_DOWNLOAD_POLICY_H_
#define CHROME_BROWSER_SAFE_BROWSING_UNVERIFIED_DOWNLOAD_POLICY_H_

namespace base {
class FilePath;
}

namespace safe_browsing {

// Returns true if |file| is allowed to be downloaded without invoking
// SafeBrowsing to verify the contents and source URL.
bool IsUnverifiedDownloadAllowed(const base::FilePath& file);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_UNVERIFIED_DOWNLOAD_POLICY_H_
