// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/substring_set_matcher.h"

#include "base/logging.h"
#include "base/stl_util.h"

namespace extensions {

//
// SubstringPattern
//

SubstringPattern::SubstringPattern(const std::string& pattern,
                                   SubstringPattern::ID id)
    : pattern_(pattern), id_(id) {}

SubstringPattern::~SubstringPattern() {}

bool SubstringPattern::operator<(const SubstringPattern& rhs) const {
  if (id_ < rhs.id_) return true;
  if (id_ > rhs.id_) return false;
  return pattern_ < rhs.pattern_;
}

//
// SubstringSetMatcher
//

SubstringSetMatcher::SubstringSetMatcher() {}

SubstringSetMatcher::~SubstringSetMatcher() {}

void SubstringSetMatcher::RegisterPatterns(
    const std::vector<const SubstringPattern*>& rules) {
  for (std::vector<const SubstringPattern*>::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    DCHECK(patterns_.find((*i)->id()) == patterns_.end());
    patterns_[(*i)->id()] = *i;
  }
}

void SubstringSetMatcher::UnregisterPatterns(
    const std::vector<const SubstringPattern*>& patterns) {
  for (std::vector<const SubstringPattern*>::const_iterator i =
      patterns.begin(); i != patterns.end(); ++i) {
    patterns_.erase((*i)->id());
  }
}

void SubstringSetMatcher::RegisterAndUnregisterPatterns(
      const std::vector<const SubstringPattern*>& to_register,
      const std::vector<const SubstringPattern*>& to_unregister) {
  // In the Aho-Corasick implementation this will change, the main
  // implementation will be here, and RegisterPatterns/UnregisterPatterns
  // will delegate to this version.
  RegisterPatterns(to_register);
  UnregisterPatterns(to_unregister);
}

bool SubstringSetMatcher::Match(const std::string& text,
                                std::set<SubstringPattern::ID>* matches) const {
  for (SubstringPatternSet::const_iterator i = patterns_.begin();
      i != patterns_.end(); ++i) {
    if (text.find(i->second->pattern()) != std::string::npos) {
      if (matches)
        matches->insert(i->second->id());
      else
        return true;
    }
  }
  return matches && !matches->empty();
}

}  // namespace extensions
