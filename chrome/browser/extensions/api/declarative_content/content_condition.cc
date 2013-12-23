// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_condition.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_content/content_constants.h"
#include "components/url_matcher/url_matcher_factory.h"

using url_matcher::URLMatcherConditionFactory;
using url_matcher::URLMatcherConditionSet;
using url_matcher::URLMatcherFactory;

namespace keys = extensions::declarative_content_constants;

namespace {
static URLMatcherConditionSet::ID g_next_id = 0;

// TODO(jyasskin): improve error messaging to give more meaningful messages
// to the extension developer.
// Error messages:
const char kExpectedDictionary[] = "A condition has to be a dictionary.";
const char kConditionWithoutInstanceType[] = "A condition had no instanceType";
const char kExpectedOtherConditionType[] = "Expected a condition of type "
    "declarativeContent.PageStateMatcher";
const char kUnknownConditionAttribute[] = "Unknown condition attribute '%s'";
const char kInvalidTypeOfParamter[] = "Attribute '%s' has an invalid type";
}  // namespace

namespace extensions {

namespace keys = declarative_content_constants;

RendererContentMatchData::RendererContentMatchData() {}
RendererContentMatchData::~RendererContentMatchData() {}

//
// ContentCondition
//

ContentCondition::ContentCondition(
    scoped_refptr<URLMatcherConditionSet> url_matcher_conditions,
    const std::vector<std::string>& css_selectors)
    : url_matcher_conditions_(url_matcher_conditions),
      css_selectors_(css_selectors) {
  CHECK(url_matcher_conditions.get());
}

ContentCondition::~ContentCondition() {}

bool ContentCondition::IsFulfilled(
    const RendererContentMatchData& renderer_data) const {
  if (!ContainsKey(renderer_data.page_url_matches,
                   url_matcher_conditions_->id()))
    return false;

  // All attributes must be fulfilled for a fulfilled condition.
  for (std::vector<std::string>::const_iterator i =
       css_selectors_.begin(); i != css_selectors_.end(); ++i) {
    if (!ContainsKey(renderer_data.css_selectors, *i))
      return false;
  }
  return true;
}

// static
scoped_ptr<ContentCondition> ContentCondition::Create(
    const Extension* extension,
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const base::Value& condition,
    std::string* error) {
  const base::DictionaryValue* condition_dict = NULL;
  if (!condition.GetAsDictionary(&condition_dict)) {
    *error = kExpectedDictionary;
    return scoped_ptr<ContentCondition>();
  }

  // Verify that we are dealing with a Condition whose type we understand.
  std::string instance_type;
  if (!condition_dict->GetString(keys::kInstanceType, &instance_type)) {
    *error = kConditionWithoutInstanceType;
    return scoped_ptr<ContentCondition>();
  }
  if (instance_type != keys::kPageStateMatcherType) {
    *error = kExpectedOtherConditionType;
    return scoped_ptr<ContentCondition>();
  }

  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set;
  std::vector<std::string> css_rules;

  for (base::DictionaryValue::Iterator iter(*condition_dict);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& condition_attribute_name = iter.key();
    const base::Value& condition_attribute_value = iter.value();
    if (condition_attribute_name == keys::kInstanceType) {
      // Skip this.
    } else if (condition_attribute_name == keys::kPageUrl) {
      const base::DictionaryValue* dict = NULL;
      if (!condition_attribute_value.GetAsDictionary(&dict)) {
        *error = base::StringPrintf(kInvalidTypeOfParamter,
                                    condition_attribute_name.c_str());
      } else {
        url_matcher_condition_set =
            URLMatcherFactory::CreateFromURLFilterDictionary(
                url_matcher_condition_factory, dict, ++g_next_id, error);
      }
    } else if (condition_attribute_name == keys::kCss) {
      const base::ListValue* css_rules_value = NULL;
      if (condition_attribute_value.GetAsList(&css_rules_value)) {
        for (size_t i = 0; i < css_rules_value->GetSize(); ++i) {
          std::string css_rule;
          if (!css_rules_value->GetString(i, &css_rule)) {
            *error = base::StringPrintf(kInvalidTypeOfParamter,
                                        condition_attribute_name.c_str());
            break;
          }
          css_rules.push_back(css_rule);
        }
      } else {
        *error = base::StringPrintf(kInvalidTypeOfParamter,
                                    condition_attribute_name.c_str());
      }
    } else {
      *error = base::StringPrintf(kUnknownConditionAttribute,
                                  condition_attribute_name.c_str());
    }
    if (!error->empty())
      return scoped_ptr<ContentCondition>();
  }

  if (!url_matcher_condition_set.get()) {
    URLMatcherConditionSet::Conditions url_matcher_conditions;
    url_matcher_conditions.insert(
        url_matcher_condition_factory->CreateHostPrefixCondition(
            std::string()));
    url_matcher_condition_set =
        new URLMatcherConditionSet(++g_next_id, url_matcher_conditions);
  }
  return scoped_ptr<ContentCondition>(
      new ContentCondition(url_matcher_condition_set, css_rules));
}

}  // namespace extensions
