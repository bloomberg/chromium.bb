// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/url_pattern_set.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/values.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/url_pattern.h"
#include "googleurl/src/gurl.h"

namespace extensions {

namespace {

const char kInvalidURLPatternError[] = "Invalid url pattern '*'";

}  // namespace

// static
void URLPatternSet::CreateDifference(const URLPatternSet& set1,
                                     const URLPatternSet& set2,
                                     URLPatternSet* out) {
  out->ClearPatterns();
  std::set_difference(set1.patterns_.begin(), set1.patterns_.end(),
                      set2.patterns_.begin(), set2.patterns_.end(),
                      std::inserter<std::set<URLPattern> >(
                          out->patterns_, out->patterns_.begin()));
}

// static
void URLPatternSet::CreateIntersection(const URLPatternSet& set1,
                                       const URLPatternSet& set2,
                                       URLPatternSet* out) {
  out->ClearPatterns();
  std::set_intersection(set1.patterns_.begin(), set1.patterns_.end(),
                        set2.patterns_.begin(), set2.patterns_.end(),
                        std::inserter<std::set<URLPattern> >(
                            out->patterns_, out->patterns_.begin()));
}

// static
void URLPatternSet::CreateUnion(const URLPatternSet& set1,
                                const URLPatternSet& set2,
                                URLPatternSet* out) {
  out->ClearPatterns();
  std::set_union(set1.patterns_.begin(), set1.patterns_.end(),
                 set2.patterns_.begin(), set2.patterns_.end(),
                 std::inserter<std::set<URLPattern> >(
                     out->patterns_, out->patterns_.begin()));
}

// static
void URLPatternSet::CreateUnion(const std::vector<URLPatternSet>& sets,
                                URLPatternSet* out) {
  out->ClearPatterns();
  if (sets.empty())
    return;

  // N-way union algorithm is basic O(nlog(n)) merge algorithm.
  //
  // Do the first merge step into a working set so that we don't mutate any of
  // the input.
  std::vector<URLPatternSet> working;
  for (size_t i = 0; i < sets.size(); i += 2) {
    if (i + 1 < sets.size()) {
      URLPatternSet u;
      URLPatternSet::CreateUnion(sets[i], sets[i + 1], &u);
      working.push_back(u);
    } else {
      working.push_back(sets[i]);
    }
  }

  for (size_t skip = 1; skip < working.size(); skip *= 2) {
    for (size_t i = 0; i < (working.size() - skip); i += skip) {
      URLPatternSet u;
      URLPatternSet::CreateUnion(working[i], working[i + skip], &u);
      working[i].patterns_.swap(u.patterns_);
    }
  }

  out->patterns_.swap(working[0].patterns_);
}

URLPatternSet::URLPatternSet() {}

URLPatternSet::URLPatternSet(const URLPatternSet& rhs)
    : patterns_(rhs.patterns_) {}

URLPatternSet::URLPatternSet(const std::set<URLPattern>& patterns)
    : patterns_(patterns) {}

URLPatternSet::~URLPatternSet() {}

URLPatternSet& URLPatternSet::operator=(const URLPatternSet& rhs) {
  patterns_ = rhs.patterns_;
  return *this;
}

bool URLPatternSet::operator==(const URLPatternSet& other) const {
  return patterns_ == other.patterns_;
}

bool URLPatternSet::is_empty() const {
  return patterns_.empty();
}

size_t URLPatternSet::size() const {
  return patterns_.size();
}

bool URLPatternSet::AddPattern(const URLPattern& pattern) {
  return patterns_.insert(pattern).second;
}

void URLPatternSet::AddPatterns(const URLPatternSet& set) {
  patterns_.insert(set.patterns().begin(),
                   set.patterns().end());
}

void URLPatternSet::ClearPatterns() {
  patterns_.clear();
}

bool URLPatternSet::Contains(const URLPatternSet& other) const {
  for (URLPatternSet::const_iterator it = other.begin();
       it != other.end(); ++it) {
    if (!ContainsPattern(*it))
      return false;
  }

  return true;
}

bool URLPatternSet::MatchesURL(const GURL& url) const {
  for (URLPatternSet::const_iterator pattern = patterns_.begin();
       pattern != patterns_.end(); ++pattern) {
    if (pattern->MatchesURL(url))
      return true;
  }

  return false;
}

bool URLPatternSet::MatchesSecurityOrigin(const GURL& origin) const {
  for (URLPatternSet::const_iterator pattern = patterns_.begin();
       pattern != patterns_.end(); ++pattern) {
    if (pattern->MatchesSecurityOrigin(origin))
      return true;
  }

  return false;
}

bool URLPatternSet::OverlapsWith(const URLPatternSet& other) const {
  // Two extension extents overlap if there is any one URL that would match at
  // least one pattern in each of the extents.
  for (URLPatternSet::const_iterator i = patterns_.begin();
       i != patterns_.end(); ++i) {
    for (URLPatternSet::const_iterator j = other.patterns().begin();
         j != other.patterns().end(); ++j) {
      if (i->OverlapsWith(*j))
        return true;
    }
  }

  return false;
}

scoped_ptr<base::ListValue> URLPatternSet::ToValue() const {
  scoped_ptr<ListValue> value(new ListValue);
  for (URLPatternSet::const_iterator i = patterns_.begin();
       i != patterns_.end(); ++i)
    value->AppendIfNotPresent(Value::CreateStringValue(i->GetAsString()));
  return value.Pass();
}

bool URLPatternSet::Populate(const std::vector<std::string>& patterns,
                             int valid_schemes,
                             bool allow_file_access,
                             std::string* error) {
  ClearPatterns();
  for (size_t i = 0; i < patterns.size(); ++i) {
    URLPattern pattern(valid_schemes);
    if (pattern.Parse(patterns[i]) != URLPattern::PARSE_SUCCESS) {
      if (error) {
        *error = ErrorUtils::FormatErrorMessage(kInvalidURLPatternError,
                                                patterns[i]);
      } else {
        LOG(ERROR) << "Invalid url pattern: " << patterns[i];
      }
      return false;
    }
    if (!allow_file_access && pattern.MatchesScheme(chrome::kFileScheme)) {
      pattern.SetValidSchemes(
          pattern.valid_schemes() & ~URLPattern::SCHEME_FILE);
    }
    AddPattern(pattern);
  }
  return true;
}

bool URLPatternSet::Populate(const base::ListValue& value,
                             int valid_schemes,
                             bool allow_file_access,
                             std::string* error) {
  std::vector<std::string> patterns;
  for (size_t i = 0; i < value.GetSize(); ++i) {
    std::string item;
    if (!value.GetString(i, &item))
      return false;
    patterns.push_back(item);
  }
  return Populate(patterns, valid_schemes, allow_file_access, error);
}

bool URLPatternSet::ContainsPattern(const URLPattern& pattern) const {
  for (URLPatternSet::const_iterator it = begin();
       it != end(); ++it) {
    if (it->Contains(pattern))
      return true;
  }
  return false;
}

}  // namespace extensions
