// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_SUBSTRING_SET_MATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_SUBSTRING_SET_MATCHER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"

namespace extensions {

// An individual pattern of the SubstringSetMatcher. A pattern consists of
// a string (interpreted as individual bytes, no character encoding) and an
// identifier.
// Each pattern that matches a string emits its ID to the caller of
// SubstringSetMatcher::Match(). This helps the caller to figure out what
// patterns matched a string. All patterns registered to a SubstringSetMatcher
// need to contain unique IDs.
class SubstringPattern {
 public:
  typedef int ID;

  SubstringPattern(const std::string& pattern, ID id);
  ~SubstringPattern();
  const std::string& pattern() const { return pattern_; }
  ID id() const { return id_; }

  bool operator<(const SubstringPattern& rhs) const;

 private:
  std::string pattern_;
  ID id_;

  DISALLOW_COPY_AND_ASSIGN(SubstringPattern);
};

// Class that store a set of string patterns and can find for a string S,
// which string patterns occur in S.
class SubstringSetMatcher {
 public:
  SubstringSetMatcher();
  ~SubstringSetMatcher();

  // Registers all |patterns|. The ownership remains with the caller.
  // The same pattern cannot be registered twice and each pattern needs to have
  // a unique ID.
  // Ownership of the patterns remains with the caller.
  void RegisterPatterns(const std::vector<const SubstringPattern*>& patterns);

  // Unregisters the passed |patterns|.
  void UnregisterPatterns(const std::vector<const SubstringPattern*>& patterns);

  // Analogous to RegisterPatterns and UnregisterPatterns but executes both
  // operations in one step, which may be cheaper in the execution.
  void RegisterAndUnregisterPatterns(
      const std::vector<const SubstringPattern*>& to_register,
      const std::vector<const SubstringPattern*>& to_unregister);

  // Matches |text| against all registered SubstringPatterns. Stores the IDs
  // of matching patterns in |matches|. |matches| is not cleared before adding
  // to it.
  bool Match(const std::string& text,
             std::set<SubstringPattern::ID>* matches) const;

 private:
  typedef std::map<SubstringPattern::ID, const SubstringPattern*>
      SubstringPatternSet;
  SubstringPatternSet patterns_;

  DISALLOW_COPY_AND_ASSIGN(SubstringSetMatcher);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_SUBSTRING_SET_MATCHER_H_
