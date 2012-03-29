// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stages.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition_attribute.h"
#include "net/url_request/url_request.h"

namespace {
static extensions::URLMatcherConditionSet::ID g_next_id = 0;

// TODO(battre): improve error messaging to give more meaningful messages
// to the extension developer.
// Error messages:
const char kExpectedDictionary[] = "A condition has to be a dictionary.";
const char kConditionWithoutInstanceType[] = "A condition had no instanceType";
const char kExpectedOtherConditionType[] = "Expected a condition of type "
    "experimental.webRequest.RequestMatcher";
const char kUnknownConditionAttribute[] = "UnKnown condition attribute '%s'";
const char kConditionExpectedString[] =
    "Condition '%s' expected a string value";

// String literals from the JavaScript API:
const char kRequestMatcher[] = "experimental.webRequest.RequestMatcher";
const char kInstanceType[] = "instanceType";

const char kHostContainsKey[] = "host_contains";
const char kHostEqualsKey[] = "host_equals";
const char kHostPrefixKey[] = "host_prefix";
const char kHostSuffixKey[] = "host_suffix";
const char kHostSuffixPathPrefixKey[] = "host_suffix_path_prefix";
const char kPathContainsKey[] = "path_contains";
const char kPathEqualsKey[] = "path_equals";
const char kPathPrefixKey[] = "path_prefix";
const char kPathSuffixKey[] = "path_suffix";
const char kQueryContainsKey[] = "query_contains";
const char kQueryEqualsKey[] = "query_equals";
const char kQueryPrefixKey[] = "query_prefix";
const char kQuerySuffixKey[] = "query_suffix";
const char kURLContainsKey[] = "url_contains";
const char kURLEqualsKey[] = "url_equals";
const char kURLPrefixKey[] = "url_prefix";
const char kURLSuffixKey[] = "url_suffix";

// Registry for all factory methods of extensions::URLMatcherConditionFactory
// that allows translating string literals from the extension API into
// the corresponding factory method to be called.
class URLMatcherConditionFactoryMethods {
 public:
  URLMatcherConditionFactoryMethods() {
    typedef extensions::URLMatcherConditionFactory F;
    factory_methods_[kHostContainsKey] = &F::CreateHostContainsCondition;
    factory_methods_[kHostEqualsKey] = &F::CreateHostEqualsCondition;
    factory_methods_[kHostPrefixKey] = &F::CreateHostPrefixCondition;
    factory_methods_[kHostSuffixKey] = &F::CreateHostSuffixCondition;
    factory_methods_[kPathContainsKey] = &F::CreatePathContainsCondition;
    factory_methods_[kPathEqualsKey] = &F::CreatePathEqualsCondition;
    factory_methods_[kPathPrefixKey] = &F::CreatePathPrefixCondition;
    factory_methods_[kPathSuffixKey] = &F::CreatePathSuffixCondition;
    factory_methods_[kQueryContainsKey] = &F::CreateQueryContainsCondition;
    factory_methods_[kQueryEqualsKey] = &F::CreateQueryEqualsCondition;
    factory_methods_[kQueryPrefixKey] = &F::CreateQueryPrefixCondition;
    factory_methods_[kQuerySuffixKey] = &F::CreateQuerySuffixCondition;
    factory_methods_[kURLContainsKey] = &F::CreateURLContainsCondition;
    factory_methods_[kURLEqualsKey] = &F::CreateURLEqualsCondition;
    factory_methods_[kURLPrefixKey] = &F::CreateURLPrefixCondition;
    factory_methods_[kURLSuffixKey] = &F::CreateURLSuffixCondition;
  }

  // Returns whether a factory method for the specified |pattern_type| (e.g.
  // "host_suffix") is known.
  bool Contains(const std::string& pattern_type) const {
    return factory_methods_.find(pattern_type) != factory_methods_.end();
  }

  // Creates a URLMatcherCondition instance from |url_matcher_condition_factory|
  // of the given |pattern_type| (e.g. "host_suffix") for the given
  // |pattern_value| (e.g. "example.com").
  // The |pattern_type| needs to be known to this class (see Contains()) or
  // a CHECK is triggered.
  extensions::URLMatcherCondition Call(
      extensions::URLMatcherConditionFactory* url_matcher_condition_factory,
      const std::string& pattern_type,
      const std::string& pattern_value) const {
    FactoryMethods::const_iterator i = factory_methods_.find(pattern_type);
    CHECK(i != factory_methods_.end());
    const FactoryMethod& method = i->second;
    return (url_matcher_condition_factory->*method)(pattern_value);
  }

 private:
  typedef extensions::URLMatcherCondition
      (extensions::URLMatcherConditionFactory::* FactoryMethod)
      (const std::string& prefix);
  typedef std::map<std::string, FactoryMethod> FactoryMethods;

  FactoryMethods factory_methods_;

  DISALLOW_COPY_AND_ASSIGN(URLMatcherConditionFactoryMethods);
};

static base::LazyInstance<URLMatcherConditionFactoryMethods>
    g_url_matcher_condition_factory_methods = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

//
// WebRequestCondition
//

WebRequestCondition::WebRequestCondition(
    const URLMatcherConditionSet& url_matcher_conditions,
    const WebRequestConditionAttributes& condition_attributes)
    : url_matcher_conditions_(url_matcher_conditions),
      condition_attributes_(condition_attributes) {}

WebRequestCondition::~WebRequestCondition() {}

bool WebRequestCondition::IsFulfilled(net::URLRequest* request) const {
  // All condition attributes must be fulfilled for a fulfilled condition.
  for (WebRequestConditionAttributes::const_iterator i =
       condition_attributes_.begin(); i != condition_attributes_.end(); ++i) {
    if (!(*i)->IsFulfilled(request))
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
  if (!condition_dict->GetString(kInstanceType, &instance_type)) {
    *error = kConditionWithoutInstanceType;
    return scoped_ptr<WebRequestCondition>(NULL);
  }
  if (instance_type != kRequestMatcher) {
    *error = kExpectedOtherConditionType;
    return scoped_ptr<WebRequestCondition>(NULL);
  }

  WebRequestConditionAttributes attributes;
  URLMatcherConditionSet::Conditions url_matcher_conditions;

  for (base::DictionaryValue::Iterator iter(*condition_dict);
       iter.HasNext(); iter.Advance()) {
    const std::string& condition_attribute_name = iter.key();
    const Value& condition_attribute_value = iter.value();
    if (condition_attribute_name == kInstanceType) {
      // Skip this.
    } else if (IsURLMatcherConditionAttribute(condition_attribute_name)) {
      URLMatcherCondition url_matcher_condition =
          CreateURLMatcherCondition(
              url_matcher_condition_factory,
              condition_attribute_name,
              &condition_attribute_value,
              error);
      if (!error->empty())
        return scoped_ptr<WebRequestCondition>(NULL);
      url_matcher_conditions.insert(url_matcher_condition);
    } else if (WebRequestConditionAttribute::IsKnownType(
        condition_attribute_name)) {
      scoped_ptr<WebRequestConditionAttribute> attribute =
          WebRequestConditionAttribute::Create(
              condition_attribute_name,
              &condition_attribute_value,
              error);
      if (!error->empty())
        return scoped_ptr<WebRequestCondition>(NULL);
      attributes.push_back(make_linked_ptr(attribute.release()));
    } else {
      *error = base::StringPrintf(kUnknownConditionAttribute,
                                  condition_attribute_name.c_str());
      return scoped_ptr<WebRequestCondition>(NULL);
    }
  }

  // As the URL is the preliminary matching criterion that triggers the tests
  // for the remaining condition attributes, we insert an empty URL match if
  // no other url match conditions were specified. Such an empty URL is always
  // matched.
  if (url_matcher_conditions.empty()) {
    url_matcher_conditions.insert(
        url_matcher_condition_factory->CreateHostPrefixCondition(""));
  }

  URLMatcherConditionSet url_matcher_condition_set(++g_next_id,
                                                   url_matcher_conditions);
  return scoped_ptr<WebRequestCondition>(
      new WebRequestCondition(url_matcher_condition_set, attributes));
}

// static
bool WebRequestCondition::IsURLMatcherConditionAttribute(
    const std::string& condition_attribute_name) {
  return g_url_matcher_condition_factory_methods.Get().Contains(
      condition_attribute_name);
}

// static
URLMatcherCondition WebRequestCondition::CreateURLMatcherCondition(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const std::string& condition_attribute_name,
    const base::Value* value,
    std::string* error) {
  std::string str_value;
  if (!value->GetAsString(&str_value)) {
    *error = base::StringPrintf(kConditionExpectedString,
                                condition_attribute_name.c_str());
    return URLMatcherCondition();
  }
  return g_url_matcher_condition_factory_methods.Get().Call(
      url_matcher_condition_factory, condition_attribute_name, str_value);
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
    net::URLRequest* request) const {
  MatchTriggers::const_iterator trigger = match_triggers_.find(url_match);
  DCHECK(trigger != match_triggers_.end());
  DCHECK_EQ(url_match, trigger->second->url_matcher_condition_set_id());
  return trigger->second->IsFulfilled(request);
}

void WebRequestConditionSet::GetURLMatcherConditionSets(
    std::vector<URLMatcherConditionSet>* condition_sets) const {
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
