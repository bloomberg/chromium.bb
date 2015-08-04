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

// Tests the URL of a page against conditions specified with the
// URLMatcherConditionSet.
class DeclarativeContentPageUrlPredicate {
 public:
  explicit DeclarativeContentPageUrlPredicate(
      scoped_refptr<url_matcher::URLMatcherConditionSet>
          url_matcher_condition_set);
  ~DeclarativeContentPageUrlPredicate();

  url_matcher::URLMatcherConditionSet* url_matcher_condition_set() const {
    return url_matcher_condition_set_.get();
  }

  // Evaluate for match IDs for the URL of the top-level page of the renderer.
  bool Evaluate(
      const std::set<url_matcher::URLMatcherConditionSet::ID>&
          page_url_matches) const;

 private:
  scoped_refptr<url_matcher::URLMatcherConditionSet> url_matcher_condition_set_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentPageUrlPredicate);
};

// Tests whether all the specified CSS selectors match on the page.
class DeclarativeContentCssPredicate {
 public:
  explicit DeclarativeContentCssPredicate(
      const std::vector<std::string>& css_selectors);
  ~DeclarativeContentCssPredicate();

  const std::vector<std::string>& css_selectors() const {
    return css_selectors_;
  }

  // Evaluate for all watched CSS selectors that match on frames with the same
  // origin as the page's main frame.
  bool Evaluate(const base::hash_set<std::string>& matched_css_selectors) const;

 private:
  std::vector<std::string> css_selectors_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentCssPredicate);
};

// Tests the bookmarked state of the page.
class DeclarativeContentIsBookmarkedPredicate {
 public:
  DeclarativeContentIsBookmarkedPredicate(
      scoped_refptr<const Extension> extension,
      bool is_bookmarked);
  ~DeclarativeContentIsBookmarkedPredicate();

  bool IsIgnored() const;
  // Evaluate for URL bookmarked state.
  bool Evaluate(bool url_is_bookmarked) const;

 private:
  scoped_refptr<const Extension> extension_;
  bool is_bookmarked_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentIsBookmarkedPredicate);
};

scoped_ptr<DeclarativeContentPageUrlPredicate> CreatePageUrlPredicate(
    const base::Value& value,
    url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
    std::string* error);

scoped_ptr<DeclarativeContentCssPredicate> CreateCssPredicate(
    const base::Value& value,
    std::string* error);

scoped_ptr<DeclarativeContentIsBookmarkedPredicate> CreateIsBookmarkedPredicate(
    const base::Value& value,
    const Extension* extension,
    std::string* error);

// Representation of a condition in the Declarative Content API. A condition
// consists of a set of predicates on the page state, all of which must be
// satisified for the condition to be fulfilled.
struct ContentCondition {
 public:
  ContentCondition(
      scoped_ptr<DeclarativeContentPageUrlPredicate> page_url_predicate,
      scoped_ptr<DeclarativeContentCssPredicate> css_predicate,
      scoped_ptr<DeclarativeContentIsBookmarkedPredicate>
          is_bookmarked_predicate);
  ~ContentCondition();

  scoped_ptr<DeclarativeContentPageUrlPredicate> page_url_predicate;
  scoped_ptr<DeclarativeContentCssPredicate> css_predicate;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> is_bookmarked_predicate;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentCondition);
};

// Factory function that instantiates a ContentCondition according to the
// description |condition| passed by the extension API.  |condition| should be
// an instance of declarativeContent.PageStateMatcher.
scoped_ptr<ContentCondition> CreateContentCondition(
    scoped_refptr<const Extension> extension,
    url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
    const base::Value& condition,
    std::string* error);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_CONDITION_H_
