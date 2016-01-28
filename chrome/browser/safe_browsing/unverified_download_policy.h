// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_UNVERIFIED_DOWNLOAD_POLICY_H_
#define CHROME_BROWSER_SAFE_BROWSING_UNVERIFIED_DOWNLOAD_POLICY_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"

class GURL;

namespace safe_browsing {

enum class UnverifiedDownloadPolicy { ALLOWED, DISALLOWED };

using UnverifiedDownloadCheckCompletionCallback =
    base::Callback<void(UnverifiedDownloadPolicy)>;

// Invokes |callback| on the current thread with the effective download policy
// for an unverified download of |file| by |requestor|. If it is possible for
// the file to be downloaded with alternate file extensions, they should be
// specified in |alternate_extensions|.
void CheckUnverifiedDownloadPolicy(
    const GURL& requestor,
    const base::FilePath& file,
    const std::vector<base::FilePath::StringType>& alternate_extensions,
    const UnverifiedDownloadCheckCompletionCallback& callback);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_UNVERIFIED_DOWNLOAD_POLICY_H_
