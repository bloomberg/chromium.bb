// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions to check executable signatures for malicious binary
// detection.  Each platform has its own implementation of this class.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SIGNATURE_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_SIGNATURE_UTIL_H_
#pragma once

class FilePath;

namespace safe_browsing {
namespace signature_util {

// Returns true if the given file path contains a signature.  No checks are
// performed as to the validity of the signature.
bool IsSigned(const FilePath& file_path);

}  // namespace signature_util
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SIGNATURE_UTIL_H_
