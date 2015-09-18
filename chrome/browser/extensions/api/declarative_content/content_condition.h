// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_CONDITION_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_CONDITION_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/extensions/api/declarative_content/content_predicate_evaluator.h"

namespace base {
class Value;
}

namespace extensions {

class Extension;

// Representation of a condition in the Declarative Content API. A condition
// consists of a set of predicates on the page state, all of which must be
// satisified for the condition to be fulfilled.
//
// For example, given the sample code at
// https://developer.chrome.com/extensions/declarativeContent#rules, the entity
// rule1['conditions'][0] is represented by a ContentCondition.
struct ContentCondition {
 public:
  explicit ContentCondition(ScopedVector<const ContentPredicate> predicates);
  ~ContentCondition();

  ScopedVector<const ContentPredicate> predicates;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentCondition);
};

// Factory function that instantiates a ContentCondition according to the
// description |condition|, which should be an instance of
// declarativeContent.PageStateMatcher from the Declarative Content
// API. |predicate_factories| maps attribute names in the API to factories that
// create the corresponding predicate.
scoped_ptr<ContentCondition> CreateContentCondition(
    const Extension* extension,
    const std::map<std::string, ContentPredicateFactory*>& predicate_factories,
    const base::Value& condition,
    std::string* error);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_CONDITION_H_
