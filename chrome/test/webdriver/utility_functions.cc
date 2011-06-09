// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/utility_functions.h"

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"

namespace webdriver {

std::string GenerateRandomID() {
  uint64 msb = base::RandUint64();
  uint64 lsb = base::RandUint64();
  return base::StringPrintf("%016" PRIx64 "%016" PRIx64, msb, lsb);
}

}  // namespace webdriver
