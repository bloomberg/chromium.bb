// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/murmurhash3_util.h"
#include "third_party/smhasher/src/MurmurHash3.h"

namespace safe_browsing {

uint32 MurmurHash3String(const std::string& str, uint32 seed) {
  uint32 output;
  MurmurHash3_x86_32(str.data(), str.size(), seed, &output);
  return output;
}

}  // namespace safe_browsing
