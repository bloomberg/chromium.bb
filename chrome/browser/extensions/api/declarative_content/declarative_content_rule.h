// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_RULE_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_RULE_H__

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/declarative_content/content_action.h"
#include "chrome/browser/extensions/api/declarative_content/content_condition.h"

namespace extensions {

class Extension;

using DeclarativeContentConditions =
    std::vector<linked_ptr<const ContentCondition>>;
using DeclarativeContentActions =
    std::vector<scoped_refptr<const ContentAction>>;

// Representation of a rule of a declarative API:
// https://developer.chrome.com/beta/extensions/events.html#declarative.
// Generally a RulesRegistry will hold a collection of Rules for a given
// declarative API and contain the logic for matching and applying them.
struct DeclarativeContentRule {
 public:
  DeclarativeContentRule();
  ~DeclarativeContentRule();

  const Extension* extension;
  DeclarativeContentConditions conditions;
  DeclarativeContentActions actions;
  int priority;

 private:
  // This struct is not intended to be copied.
  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentRule);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_RULE_H__
