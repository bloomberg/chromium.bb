// Copyrights 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Safe Browsing utility functions.

#ifndef COMPONENTS_SAFE_BROWSING_COMMON_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_COMMON_UTILS_H_

#include "url/gurl.h"

namespace safe_browsing {

// Shorten URL by replacing its contents with its SHA256 hash if it has data
// scheme.
std::string ShortURLForReporting(const GURL& url);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_COMMON_UTILS_H_
