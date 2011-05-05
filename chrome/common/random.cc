// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/random.h"

#include <string>

#include "base/base64.h"
#include "base/rand_util.h"

std::string Generate128BitRandomBase64String() {
  const int kNumberBytes = 128 / 8;
  std::string base64_encoded_bytes;
  base::Base64Encode(base::RandBytesAsString(kNumberBytes),
                     &base64_encoded_bytes);
  return base64_encoded_bytes;
}
