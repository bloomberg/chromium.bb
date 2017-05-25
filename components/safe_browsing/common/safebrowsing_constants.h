// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_COMMON_SAFEBROWSING_CONSTANTS_H_
#define COMPONENTS_SAFE_BROWSING_COMMON_SAFEBROWSING_CONSTANTS_H_

#include "base/files/file_path.h"

namespace safe_browsing {

extern const base::FilePath::CharType kSafeBrowsingBaseFilename[];

// Filename suffix for the cookie database.
extern const base::FilePath::CharType kCookiesFile[];
extern const base::FilePath::CharType kChannelIDFile[];
}

#endif  // COMPONENTS_SAFE_BROWSING_COMMON_SAFEBROWSING_CONSTANTS_H_
