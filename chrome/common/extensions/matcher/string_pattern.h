// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MATCHER_STRING_PATTERN_H_
#define CHROME_COMMON_EXTENSIONS_MATCHER_STRING_PATTERN_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace extensions {

// An individual pattern of a substring or regex matcher. A pattern consists of
// a string (interpreted as individual bytes, no character encoding) and an
// identifier.
// IDs are returned to the caller of SubstringSetMatcher::Match() or
// RegexMatcher::MatchURL() to help the caller to figure out what
// patterns matched a string. All patterns registered to a matcher
// need to contain unique IDs.
class StringPattern {
 public:
  typedef int ID;

  StringPattern(const std::string& pattern, ID id);
  ~StringPattern();
  const std::string& pattern() const { return pattern_; }
  ID id() const { return id_; }

  bool operator<(const StringPattern& rhs) const;

 private:
  std::string pattern_;
  ID id_;

  DISALLOW_COPY_AND_ASSIGN(StringPattern);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MATCHER_STRING_PATTERN_H_
