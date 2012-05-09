// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/matcher/url_matcher_factory.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/extensions/matcher/url_matcher_constants.h"
#include "chrome/common/extensions/matcher/url_matcher_helpers.h"

namespace helpers = extensions::url_matcher_helpers;
namespace keys = extensions::url_matcher_constants;

namespace {
// Error messages:
const char kInvalidPortRanges[] = "Invalid port ranges in UrlFilter.";
const char kVectorOfStringsExpected[] =
    "UrlFilter attribute '%s' expected a vector of strings as parameter.";
const char kUnknownURLFilterAttribute[] =
    "Unknown attribute '%s' in UrlFilter.";
const char kAttributeExpectedString[] =
    "UrlFilter attribute '%s' expected a string value.";

// Registry for all factory methods of extensions::URLMatcherConditionFactory
// that allows translating string literals from the extension API into
// the corresponding factory method to be called.
class URLMatcherConditionFactoryMethods {
 public:
  URLMatcherConditionFactoryMethods() {
    typedef extensions::URLMatcherConditionFactory F;
    factory_methods_[keys::kHostContainsKey] = &F::CreateHostContainsCondition;
    factory_methods_[keys::kHostEqualsKey] = &F::CreateHostEqualsCondition;
    factory_methods_[keys::kHostPrefixKey] = &F::CreateHostPrefixCondition;
    factory_methods_[keys::kHostSuffixKey] = &F::CreateHostSuffixCondition;
    factory_methods_[keys::kPathContainsKey] = &F::CreatePathContainsCondition;
    factory_methods_[keys::kPathEqualsKey] = &F::CreatePathEqualsCondition;
    factory_methods_[keys::kPathPrefixKey] = &F::CreatePathPrefixCondition;
    factory_methods_[keys::kPathSuffixKey] = &F::CreatePathSuffixCondition;
    factory_methods_[keys::kQueryContainsKey] =
        &F::CreateQueryContainsCondition;
    factory_methods_[keys::kQueryEqualsKey] = &F::CreateQueryEqualsCondition;
    factory_methods_[keys::kQueryPrefixKey] = &F::CreateQueryPrefixCondition;
    factory_methods_[keys::kQuerySuffixKey] = &F::CreateQuerySuffixCondition;
    factory_methods_[keys::kURLContainsKey] = &F::CreateURLContainsCondition;
    factory_methods_[keys::kURLEqualsKey] = &F::CreateURLEqualsCondition;
    factory_methods_[keys::kURLPrefixKey] = &F::CreateURLPrefixCondition;
    factory_methods_[keys::kURLSuffixKey] = &F::CreateURLSuffixCondition;
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

// static
scoped_refptr<URLMatcherConditionSet>
URLMatcherFactory::CreateFromURLFilterDictionary(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const base::DictionaryValue* url_filter_dict,
    URLMatcherConditionSet::ID id,
    std::string* error) {
  scoped_ptr<URLMatcherSchemeFilter> url_matcher_schema_filter;
  scoped_ptr<URLMatcherPortFilter> url_matcher_port_filter;
  URLMatcherConditionSet::Conditions url_matcher_conditions;

  for (base::DictionaryValue::Iterator iter(*url_filter_dict);
       iter.HasNext(); iter.Advance()) {
    const std::string& condition_attribute_name = iter.key();
    const Value& condition_attribute_value = iter.value();
    if (IsURLMatcherConditionAttribute(condition_attribute_name)) {
      // Handle {host, path, ...}{Prefix, Suffix, Contains, Equals}.
      URLMatcherCondition url_matcher_condition =
          CreateURLMatcherCondition(
              url_matcher_condition_factory,
              condition_attribute_name,
              &condition_attribute_value,
              error);
      if (!error->empty())
        return scoped_refptr<URLMatcherConditionSet>(NULL);
      url_matcher_conditions.insert(url_matcher_condition);
    } else if (condition_attribute_name == keys::kSchemesKey) {
      // Handle scheme.
      url_matcher_schema_filter = CreateURLMatcherScheme(
          &condition_attribute_value, error);
      if (!error->empty())
        return scoped_refptr<URLMatcherConditionSet>(NULL);
    } else if (condition_attribute_name == keys::kPortsKey) {
      // Handle ports.
      url_matcher_port_filter = CreateURLMatcherPorts(
          &condition_attribute_value, error);
      if (!error->empty())
        return scoped_refptr<URLMatcherConditionSet>(NULL);
    } else {
      // Handle unknown attributes.
      *error = base::StringPrintf(kUnknownURLFilterAttribute,
                                  condition_attribute_name.c_str());
      return scoped_refptr<URLMatcherConditionSet>(NULL);
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

  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set(
      new URLMatcherConditionSet(id, url_matcher_conditions,
          url_matcher_schema_filter.Pass(), url_matcher_port_filter.Pass()));
  return url_matcher_condition_set;
}

// static
bool URLMatcherFactory::IsURLMatcherConditionAttribute(
    const std::string& condition_attribute_name) {
  return g_url_matcher_condition_factory_methods.Get().Contains(
      condition_attribute_name);
}

// static
URLMatcherCondition URLMatcherFactory::CreateURLMatcherCondition(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const std::string& condition_attribute_name,
    const base::Value* value,
    std::string* error) {
  std::string str_value;
  if (!value->GetAsString(&str_value)) {
    *error = base::StringPrintf(kAttributeExpectedString,
                                condition_attribute_name.c_str());
    return URLMatcherCondition();
  }
  return g_url_matcher_condition_factory_methods.Get().Call(
      url_matcher_condition_factory, condition_attribute_name, str_value);
}

// static
scoped_ptr<URLMatcherSchemeFilter> URLMatcherFactory::CreateURLMatcherScheme(
    const base::Value* value,
    std::string* error) {
  std::vector<std::string> schemas;
  if (!helpers::GetAsStringVector(value, &schemas)) {
    *error = base::StringPrintf(kVectorOfStringsExpected, keys::kSchemesKey);
    return scoped_ptr<URLMatcherSchemeFilter>(NULL);
  }
  return scoped_ptr<URLMatcherSchemeFilter>(
      new URLMatcherSchemeFilter(schemas));
}

// static
scoped_ptr<URLMatcherPortFilter> URLMatcherFactory::CreateURLMatcherPorts(
    const base::Value* value,
    std::string* error) {
  std::vector<URLMatcherPortFilter::Range> ranges;
  const base::ListValue* value_list = NULL;
  if (!value->GetAsList(&value_list)) {
    *error = kInvalidPortRanges;
    return scoped_ptr<URLMatcherPortFilter>(NULL);
  }

  for (ListValue::const_iterator i = value_list->begin();
       i != value_list->end(); ++i) {
    Value* entry = *i;
    int port = 0;
    base::ListValue* range = NULL;
    if (entry->GetAsInteger(&port)) {
      ranges.push_back(URLMatcherPortFilter::CreateRange(port));
    } else if (entry->GetAsList(&range)) {
      int from = 0, to = 0;
      if (range->GetSize() != 2u ||
          !range->GetInteger(0, &from) ||
          !range->GetInteger(1, &to)) {
        *error = kInvalidPortRanges;
        return scoped_ptr<URLMatcherPortFilter>(NULL);
      }
      ranges.push_back(URLMatcherPortFilter::CreateRange(from, to));
    } else {
      *error = kInvalidPortRanges;
      return scoped_ptr<URLMatcherPortFilter>(NULL);
    }
  }

  return scoped_ptr<URLMatcherPortFilter>(new URLMatcherPortFilter(ranges));
}

}  // namespace extensions
