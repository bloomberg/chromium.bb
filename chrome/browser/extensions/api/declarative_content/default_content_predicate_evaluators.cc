// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/default_content_predicate_evaluators.h"

#include "chrome/browser/extensions/api/declarative_content/declarative_content_css_condition_tracker.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_is_bookmarked_condition_tracker.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_page_url_condition_tracker.h"

namespace extensions {

ScopedVector<ContentPredicateEvaluator> CreateDefaultContentPredicateEvaluators(
    content::BrowserContext* browser_context,
    ContentPredicateEvaluator::Delegate* delegate) {
  ScopedVector<ContentPredicateEvaluator> evaluators;
  evaluators.push_back(new DeclarativeContentPageUrlConditionTracker(delegate));
  evaluators.push_back(new DeclarativeContentCssConditionTracker(delegate));
  evaluators.push_back(new DeclarativeContentIsBookmarkedConditionTracker(
      browser_context,
      delegate));
  return evaluators.Pass();
}

}  // namespace extensions
