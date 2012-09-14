// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/matcher/string_pattern.h"

namespace extensions {

StringPattern::StringPattern(const std::string& pattern,
                             StringPattern::ID id)
    : pattern_(pattern), id_(id) {}

StringPattern::~StringPattern() {}

bool StringPattern::operator<(const StringPattern& rhs) const {
  if (id_ != rhs.id_) return id_ < rhs.id_;
  return pattern_ < rhs.pattern_;
}

}  // namespace extensions
