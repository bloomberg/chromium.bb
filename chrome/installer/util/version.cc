// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/format_macros.h"
#include "base/string_util.h"
#include "chrome/installer/util/version.h"

installer::Version::Version(int64 major, int64 minor, int64 build,
                            int64 patch)
    : major_(major),
      minor_(minor),
      build_(build),
      patch_(patch) {
  version_str_ = ASCIIToUTF16(
      StringPrintf("%" PRId64 ".%" PRId64 ".%" PRId64 ".%" PRId64,
                   major_, minor_, build_, patch_));
}

installer::Version::~Version() {
}

bool installer::Version::IsHigherThan(const installer::Version* other) const {
  return ((major_ > other->major_) ||
          ((major_ == other->major_) && (minor_ > other->minor_)) ||
          ((major_ == other->major_) && (minor_ == other->minor_)
                                     && (build_ > other->build_)) ||
          ((major_ == other->major_) && (minor_ == other->minor_)
                                     && (build_ == other->build_)
                                     && (patch_ > other->patch_)));
}

installer::Version* installer::Version::GetVersionFromString(
    const string16& version_str) {
  std::vector<string16> numbers;
  SplitString(version_str, '.', &numbers);

  if (numbers.size() != 4) {
    return NULL;
  }

  return new Version(StringToInt64(numbers[0]), StringToInt64(numbers[1]),
                     StringToInt64(numbers[2]), StringToInt64(numbers[3]));
}
