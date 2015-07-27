// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_CONDITION_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_CONDITION_H_

#include <set>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/url_matcher/url_matcher.h"

namespace base {
class Value;
}

namespace extensions {

class Extension;

struct RendererContentMatchData {
  RendererContentMatchData();
  ~RendererContentMatchData();
  // Match IDs for the URL of the top-level page the renderer is
  // returning data for.
  std::set<url_matcher::URLMatcherConditionSet::ID> page_url_matches;
  // All watched CSS selectors that match on frames with the same
  // origin as the page's main frame.
  base::hash_set<std::string> css_selectors;
  // True if the URL is bookmarked.
  bool is_bookmarked;
};

// Representation of a condition in the Declarative Content API. A condition
// consists of an optional URL and a list of CSS selectors. Each of these
// attributes needs to be fulfilled in order for the condition to be fulfilled.
//
// We distinguish between two types of attributes:
// - URL Matcher attributes are attributes that test the URL of a page.
//   These are treated separately because we use a URLMatcher to efficiently
//   test many of these attributes in parallel by using some advanced
//   data structures. The URLMatcher tells us if all URL Matcher attributes
//   are fulfilled for a ContentCondition.
// - All other conditions are represented individually as fields in
//   ContentCondition.  These conditions are probed linearly (only if the URL
//   Matcher found a hit).
//
// TODO(battre) Consider making the URLMatcher an owner of the
// URLMatcherConditionSet and only pass a pointer to URLMatcherConditionSet
// in url_matcher_condition_set(). This saves some copying in
// ContentConditionSet::GetURLMatcherConditionSets.
class ContentCondition {
 public:
  // Possible states for matching bookmarked state.
  enum BookmarkedStateMatch { NOT_BOOKMARKED, BOOKMARKED, DONT_CARE };

  ContentCondition(
      scoped_refptr<const Extension> extension,
      scoped_refptr<url_matcher::URLMatcherConditionSet>
          url_matcher_condition_set,
      const std::vector<std::string>& css_selectors,
      BookmarkedStateMatch bookmarked_state);
  ~ContentCondition();

  // Factory method that instantiates a ContentCondition according to the
  // description |condition| passed by the extension API.  |condition| should be
  // an instance of declarativeContent.PageStateMatcher.
  static scoped_ptr<ContentCondition> Create(
      scoped_refptr<const Extension> extension,
      url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value& condition,
      std::string* error);

  // Returns whether the request is a match, given that the URLMatcher found
  // a match for |url_matcher_condition_set_|.
  bool IsFulfilled(const RendererContentMatchData& renderer_data) const;

  // Returns null if there are no URLMatcher conditions.
  url_matcher::URLMatcherConditionSet* url_matcher_condition_set() const {
    return url_matcher_condition_set_.get();
  }

  // Returns the CSS selectors required to match by this condition.
  const std::vector<std::string>& css_selectors() const {
    return css_selectors_;
  }

 private:
  const scoped_refptr<const Extension> extension_;
  scoped_refptr<url_matcher::URLMatcherConditionSet> url_matcher_condition_set_;
  std::vector<std::string> css_selectors_;
  BookmarkedStateMatch bookmarked_state_;

  DISALLOW_COPY_AND_ASSIGN(ContentCondition);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_CONDITION_H_
