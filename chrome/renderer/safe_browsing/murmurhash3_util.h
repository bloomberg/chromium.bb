// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SAFE_BROWSING_MURMURHASH3_UTIL_H_
#define CHROME_RENDERER_SAFE_BROWSING_MURMURHASH3_UTIL_H_

#include <string>
#include "base/basictypes.h"

namespace safe_browsing {

// Runs the 32-bit murmurhash3 function on the given string and returns the
// output as a uint32.
uint32 MurmurHash3String(const std::string& str, uint32 seed);

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_MURMURHASH3_UTIL_H_
