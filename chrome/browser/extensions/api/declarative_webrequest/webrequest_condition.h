// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition_attribute.h"
#include "chrome/common/extensions/matcher/url_matcher.h"

namespace extensions {

// Representation of a condition in the Declarative WebRequest API. A condition
// consists of several attributes. Each of these attributes needs to be
// fulfilled in order for the condition to be fulfilled.
//
// We distinguish between two types of conditions:
// - URL Matcher conditions are conditions that test the URL of a request.
//   These are treated separately because we use a URLMatcher to efficiently
//   test many of these conditions in parallel by using some advanced
//   data structures. The URLMatcher tells us if all URL Matcher conditions
//   are fulfilled for a WebRequestCondition.
// - All other conditions are represented as WebRequestConditionAttributes.
//   These conditions are probed linearly (only if the URL Matcher found a hit).
//
// TODO(battre) Consider making the URLMatcher an owner of the
// URLMatcherConditionSet and only pass a pointer to URLMatcherConditionSet
// in url_matcher_condition_set(). This saves some copying in
// WebRequestConditionSet::GetURLMatcherConditionSets.
class WebRequestCondition {
 public:
  WebRequestCondition(
      scoped_refptr<URLMatcherConditionSet> url_matcher_conditions,
      const WebRequestConditionAttributes& condition_attributes);
  ~WebRequestCondition();

  // Factory method that instantiates a WebRequestCondition according to
  // the description |condition| passed by the extension API.
  static scoped_ptr<WebRequestCondition> Create(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value& condition,
      std::string* error);

  // Returns whether |request| is a match, given that the URLMatcher found
  // a match for |url_matcher_conditions_|.
  bool IsFulfilled(net::URLRequest* request, RequestStages request_stage) const;

  // Returns a URLMatcherConditionSet::ID which is the canonical representation
  // for all URL patterns that need to be matched by this WebRequestCondition.
  // This ID is registered in a URLMatcher that can inform us in case of a
  // match.
  URLMatcherConditionSet::ID url_matcher_condition_set_id() const {
    return url_matcher_conditions_->id();
  }

  // Returns the set of conditions that are checked on the URL. This is the
  // primary trigger for WebRequestCondition and therefore never empty.
  // (If it was empty, the URLMatcher would never notify us about network
  // requests which might fulfill the entire WebRequestCondition).
  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set() const {
    return url_matcher_conditions_;
  }

 private:
  scoped_refptr<URLMatcherConditionSet> url_matcher_conditions_;
  WebRequestConditionAttributes condition_attributes_;

  // Bit vector indicating all RequestStages during which all
  // |condition_attributes_| can be evaluated.
  int applicable_request_stages_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestCondition);
};

// This class stores a set of conditions that may be part of a WebRequestRule.
// If any condition is fulfilled, the WebRequestActions of the WebRequestRule
// can be triggered.
class WebRequestConditionSet {
 public:
  typedef std::vector<linked_ptr<json_schema_compiler::any::Any> > AnyVector;

  explicit WebRequestConditionSet(
      const std::vector<linked_ptr<WebRequestCondition> >& conditions);
  ~WebRequestConditionSet();

  // Factory method that creates an WebRequestConditionSet according to the JSON
  // array |conditions| passed by the extension API.
  // Sets |error| and returns NULL in case of an error.
  static scoped_ptr<WebRequestConditionSet> Create(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const AnyVector& conditions,
      std::string* error);

  const std::vector<linked_ptr<WebRequestCondition> >& conditions() const {
    return conditions_;
  }

  // Returns whether any condition in the condition set is fulfilled
  // based on a match |url_match| and the value of |request|. This function
  // should be called for each URLMatcherConditionSet::ID that was found
  // by the URLMatcher to ensure that the each trigger in |match_triggers_| is
  // found.
  bool IsFulfilled(URLMatcherConditionSet::ID url_match,
                   net::URLRequest* request,
                   RequestStages request_stage) const;

  // Appends the URLMatcherConditionSet from all conditions to |condition_sets|.
  void GetURLMatcherConditionSets(
      URLMatcherConditionSet::Vector* condition_sets) const;

 private:
  typedef std::vector<linked_ptr<WebRequestCondition> > Conditions;
  Conditions conditions_;

  typedef std::map<URLMatcherConditionSet::ID, WebRequestCondition*>
      MatchTriggers;
  MatchTriggers match_triggers_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionSet);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_H_
