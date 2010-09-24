// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/version.h"

#include <vector>

#include "base/format_macros.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
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
    LOG(ERROR) << "Invalid version string: " << version_str;
    return NULL;
  }

  int64 v0, v1, v2, v3;
  base::StringToInt64(numbers[0], &v0);
  base::StringToInt64(numbers[1], &v1);
  base::StringToInt64(numbers[2], &v2);
  base::StringToInt64(numbers[3], &v3);
  return new Version(v0, v1, v2, v3);
}
