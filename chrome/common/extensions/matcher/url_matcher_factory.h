// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MATCHER_URL_MATCHER_FACTORY_H_
#define CHROME_COMMON_EXTENSIONS_MATCHER_URL_MATCHER_FACTORY_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/common/extensions/matcher/url_matcher.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace extensions {

class URLMatcherFactory {
 public:
  // Creates a URLMatcherConditionSet from a UrlFilter dictionary as defined in
  // the declarative API. |url_fetcher_dict| contains the dictionary passed
  // by the extension, |id| is the identifier assigned to the created
  // URLMatcherConditionSet. In case of an error, |error| is set to contain
  // an error message.
  //
  // Note: In case this function fails or if you don't register the
  // URLMatcherConditionSet to the URLMatcher, you need to call
  // URLMatcher::ClearUnusedConditionSets() on the URLMatcher that owns this
  // URLMatcherFactory. Otherwise you leak memory.
  static scoped_refptr<URLMatcherConditionSet> CreateFromURLFilterDictionary(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::DictionaryValue* url_filter_dict,
      URLMatcherConditionSet::ID id,
      std::string* error);

 private:
  // Returns whether a condition attribute with name |condition_attribute_name|
  // needs to be handled by the URLMatcher.
  static bool IsURLMatcherConditionAttribute(
      const std::string& condition_attribute_name);

  // Factory method of for URLMatcherConditions.
  static URLMatcherCondition CreateURLMatcherCondition(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const std::string& condition_attribute_name,
      const base::Value* value,
      std::string* error);

  static scoped_ptr<URLMatcherSchemeFilter> CreateURLMatcherScheme(
      const base::Value* value, std::string* error);

  static scoped_ptr<URLMatcherPortFilter> CreateURLMatcherPorts(
      const base::Value* value, std::string* error);

  DISALLOW_IMPLICIT_CONSTRUCTORS(URLMatcherFactory);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MATCHER_URL_MATCHER_FACTORY_H_
