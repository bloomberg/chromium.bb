// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stages.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition_attribute.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/common/extensions/matcher/url_matcher.h"
#include "chrome/common/extensions/matcher/url_matcher_factory.h"
#include "net/url_request/url_request.h"

namespace keys = extensions::declarative_webrequest_constants;

namespace {
static extensions::URLMatcherConditionSet::ID g_next_id = 0;

// TODO(battre): improve error messaging to give more meaningful messages
// to the extension developer.
// Error messages:
const char kExpectedDictionary[] = "A condition has to be a dictionary.";
const char kConditionWithoutInstanceType[] = "A condition had no instanceType";
const char kExpectedOtherConditionType[] = "Expected a condition of type "
    "declarativeWebRequest.RequestMatcher";
const char kUnknownConditionAttribute[] = "Unknown condition attribute '%s'";
const char kInvalidTypeOfParamter[] = "Attribute '%s' has an invalid type";
}  // namespace

namespace extensions {

namespace keys = declarative_webrequest_constants;

//
// WebRequestCondition
//

WebRequestCondition::WebRequestCondition(
    scoped_refptr<URLMatcherConditionSet> url_matcher_conditions,
    const WebRequestConditionAttributes& condition_attributes)
    : url_matcher_conditions_(url_matcher_conditions),
      condition_attributes_(condition_attributes),
      applicable_request_stages_(~0) {
  CHECK(url_matcher_conditions.get());
  for (WebRequestConditionAttributes::const_iterator i =
       condition_attributes_.begin(); i != condition_attributes_.end(); ++i) {
    applicable_request_stages_ &= (*i)->GetStages();
  }
}

WebRequestCondition::~WebRequestCondition() {}

bool WebRequestCondition::IsFulfilled(net::URLRequest* request,
                                      RequestStages request_stage) const {
  // All condition attributes must be fulfilled for a fulfilled condition.
  if (!(request_stage & applicable_request_stages_)) {
    // A condition that cannot be evaluated is considered as violated.
    return false;
  }

  for (WebRequestConditionAttributes::const_iterator i =
       condition_attributes_.begin(); i != condition_attributes_.end(); ++i) {
    if (!(*i)->IsFulfilled(request, request_stage))
      return false;
  }
  return true;
}

// static
scoped_ptr<WebRequestCondition> WebRequestCondition::Create(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const base::Value& condition,
    std::string* error) {
  const base::DictionaryValue* condition_dict = NULL;
  if (!condition.GetAsDictionary(&condition_dict)) {
    *error = kExpectedDictionary;
    return scoped_ptr<WebRequestCondition>(NULL);
  }

  // Verify that we are dealing with a Condition whose type we understand.
  std::string instance_type;
  if (!condition_dict->GetString(keys::kInstanceTypeKey, &instance_type)) {
    *error = kConditionWithoutInstanceType;
    return scoped_ptr<WebRequestCondition>(NULL);
  }
  if (instance_type != keys::kRequestMatcherType) {
    *error = kExpectedOtherConditionType;
    return scoped_ptr<WebRequestCondition>(NULL);
  }

  WebRequestConditionAttributes attributes;
  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set;

  for (base::DictionaryValue::Iterator iter(*condition_dict);
       iter.HasNext(); iter.Advance()) {
    const std::string& condition_attribute_name = iter.key();
    const Value& condition_attribute_value = iter.value();
    if (condition_attribute_name == keys::kInstanceTypeKey) {
      // Skip this.
    } else if (condition_attribute_name == keys::kUrlKey) {
      const base::DictionaryValue* dict = NULL;
      if (!condition_attribute_value.GetAsDictionary(&dict)) {
        *error = base::StringPrintf(kInvalidTypeOfParamter,
                                    condition_attribute_name.c_str());
      } else {
        url_matcher_condition_set =
            URLMatcherFactory::CreateFromURLFilterDictionary(
                url_matcher_condition_factory, dict, ++g_next_id, error);
      }
    } else if (WebRequestConditionAttribute::IsKnownType(
        condition_attribute_name)) {
      scoped_ptr<WebRequestConditionAttribute> attribute =
          WebRequestConditionAttribute::Create(
              condition_attribute_name,
              &condition_attribute_value,
              error);
      if (attribute.get())
        attributes.push_back(make_linked_ptr(attribute.release()));
    } else {
      *error = base::StringPrintf(kUnknownConditionAttribute,
                                  condition_attribute_name.c_str());
    }
    if (!error->empty())
      return scoped_ptr<WebRequestCondition>(NULL);
  }

  if (!url_matcher_condition_set) {
    URLMatcherConditionSet::Conditions url_matcher_conditions;
    url_matcher_conditions.insert(
        url_matcher_condition_factory->CreateHostPrefixCondition(""));
    url_matcher_condition_set =
        new URLMatcherConditionSet(++g_next_id, url_matcher_conditions);
  }
  return scoped_ptr<WebRequestCondition>(
      new WebRequestCondition(url_matcher_condition_set, attributes));
}


//
// WebRequestConditionSet
//

WebRequestConditionSet::WebRequestConditionSet(
    const std::vector<linked_ptr<WebRequestCondition> >& conditions)
    : conditions_(conditions) {
  for (Conditions::iterator i = conditions_.begin(); i != conditions_.end();
       ++i) {
    URLMatcherConditionSet::ID trigger_id =
        (*i)->url_matcher_condition_set_id();
    match_triggers_[trigger_id] = i->get();
  }
}

WebRequestConditionSet::~WebRequestConditionSet() {}

bool WebRequestConditionSet::IsFulfilled(
    URLMatcherConditionSet::ID url_match,
    net::URLRequest* request,
    RequestStages request_stage) const {
  MatchTriggers::const_iterator trigger = match_triggers_.find(url_match);
  DCHECK(trigger != match_triggers_.end());
  DCHECK_EQ(url_match, trigger->second->url_matcher_condition_set_id());
  return trigger->second->IsFulfilled(request, request_stage);
}

void WebRequestConditionSet::GetURLMatcherConditionSets(
    URLMatcherConditionSet::Vector* condition_sets) const {
  for (Conditions::const_iterator i = conditions_.begin();
       i != conditions_.end(); ++i) {
    condition_sets->push_back((*i)->url_matcher_condition_set());
  }
}

// static
scoped_ptr<WebRequestConditionSet> WebRequestConditionSet::Create(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const AnyVector& conditions,
    std::string* error) {
  std::vector<linked_ptr<WebRequestCondition> > result;

  for (AnyVector::const_iterator i = conditions.begin();
       i != conditions.end(); ++i) {
    CHECK(i->get());
    scoped_ptr<WebRequestCondition> condition =
        WebRequestCondition::Create(url_matcher_condition_factory,
                                    (*i)->value(), error);
    if (!error->empty())
      return scoped_ptr<WebRequestConditionSet>(NULL);
    result.push_back(make_linked_ptr(condition.release()));
  }

  return scoped_ptr<WebRequestConditionSet>(new WebRequestConditionSet(result));
}

}  // namespace extensions
