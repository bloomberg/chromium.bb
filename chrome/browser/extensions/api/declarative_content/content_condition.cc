// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_condition.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_content/content_constants.h"
#include "components/url_matcher/url_matcher_factory.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

using url_matcher::URLMatcherConditionSet;

namespace extensions {

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
const char kInvalidTypeOfParameter[] = "Attribute '%s' has an invalid type";
const char kIsBookmarkedRequiresBookmarkPermission[] =
    "Property 'isBookmarked' requires 'bookmarks' permission";

bool HasBookmarkAPIPermission(const Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      APIPermission::kBookmark);
}

}  // namespace

namespace keys = declarative_content_constants;

scoped_ptr<DeclarativeContentPageUrlPredicate> CreatePageUrlPredicate(
    const base::Value& value,
    url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
    std::string* error) {
  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set;
  const base::DictionaryValue* dict = nullptr;
  if (!value.GetAsDictionary(&dict)) {
    *error = base::StringPrintf(kInvalidTypeOfParameter, keys::kPageUrl);
    return scoped_ptr<DeclarativeContentPageUrlPredicate>();
  } else {
    url_matcher_condition_set =
        url_matcher::URLMatcherFactory::CreateFromURLFilterDictionary(
            url_matcher_condition_factory, dict, ++g_next_id, error);
    if (!url_matcher_condition_set)
      return scoped_ptr<DeclarativeContentPageUrlPredicate>();
    return make_scoped_ptr(
        new DeclarativeContentPageUrlPredicate(url_matcher_condition_set));
  }
}

scoped_ptr<DeclarativeContentCssPredicate> CreateCssPredicate(
    const base::Value& value,
    std::string* error) {
  std::vector<std::string> css_rules;
  const base::ListValue* css_rules_value = nullptr;
  if (value.GetAsList(&css_rules_value)) {
    for (size_t i = 0; i < css_rules_value->GetSize(); ++i) {
      std::string css_rule;
      if (!css_rules_value->GetString(i, &css_rule)) {
        *error = base::StringPrintf(kInvalidTypeOfParameter, keys::kCss);
        return scoped_ptr<DeclarativeContentCssPredicate>();
      }
      css_rules.push_back(css_rule);
    }
  } else {
    *error = base::StringPrintf(kInvalidTypeOfParameter, keys::kCss);
    return scoped_ptr<DeclarativeContentCssPredicate>();
  }

  return !css_rules.empty() ?
      make_scoped_ptr(new DeclarativeContentCssPredicate(css_rules)) :
      scoped_ptr<DeclarativeContentCssPredicate>();
}

scoped_ptr<DeclarativeContentIsBookmarkedPredicate> CreateIsBookmarkedPredicate(
    const base::Value& value,
    const Extension* extension,
    std::string* error) {
  bool is_bookmarked = false;
  if (value.GetAsBoolean(&is_bookmarked)) {
    if (!HasBookmarkAPIPermission(extension)) {
      *error = kIsBookmarkedRequiresBookmarkPermission;
      return scoped_ptr<DeclarativeContentIsBookmarkedPredicate>();
    } else {
      return make_scoped_ptr(
          new DeclarativeContentIsBookmarkedPredicate(extension,
                                                      is_bookmarked));
    }
  } else {
    *error = base::StringPrintf(kInvalidTypeOfParameter, keys::kIsBookmarked);
    return scoped_ptr<DeclarativeContentIsBookmarkedPredicate>();
  }
}

DeclarativeContentPageUrlPredicate::DeclarativeContentPageUrlPredicate(
    scoped_refptr<url_matcher::URLMatcherConditionSet>
        url_matcher_condition_set)
    : url_matcher_condition_set_(url_matcher_condition_set) {
  DCHECK(url_matcher_condition_set);
}

DeclarativeContentPageUrlPredicate::~DeclarativeContentPageUrlPredicate() {
}

bool DeclarativeContentPageUrlPredicate::Evaluate(
    const std::set<url_matcher::URLMatcherConditionSet::ID>&
        page_url_matches) const {
  return ContainsKey(page_url_matches, url_matcher_condition_set_->id());
}

DeclarativeContentCssPredicate::DeclarativeContentCssPredicate(
    const std::vector<std::string>& css_selectors)
    : css_selectors_(css_selectors) {
  DCHECK(!css_selectors.empty());
}

DeclarativeContentCssPredicate::~DeclarativeContentCssPredicate() {
}

bool DeclarativeContentCssPredicate::Evaluate(
    const base::hash_set<std::string>& matched_css_selectors) const {
  // All attributes must be fulfilled for a fulfilled condition.
  for (const std::string& css_selector : css_selectors_) {
    if (!ContainsKey(matched_css_selectors, css_selector))
      return false;
  }

  return true;
}

DeclarativeContentIsBookmarkedPredicate::
DeclarativeContentIsBookmarkedPredicate(
    scoped_refptr<const Extension> extension,
    bool is_bookmarked)
    : extension_(extension),
      is_bookmarked_(is_bookmarked) {
}

DeclarativeContentIsBookmarkedPredicate::
~DeclarativeContentIsBookmarkedPredicate() {
}

bool DeclarativeContentIsBookmarkedPredicate::IsIgnored() const {
  return !HasBookmarkAPIPermission(extension_.get());
}

bool DeclarativeContentIsBookmarkedPredicate::Evaluate(
    bool url_is_bookmarked) const {
  return url_is_bookmarked == is_bookmarked_;
}

ContentCondition::ContentCondition(
    scoped_ptr<DeclarativeContentPageUrlPredicate> page_url_predicate,
    scoped_ptr<DeclarativeContentCssPredicate> css_predicate,
    scoped_ptr<DeclarativeContentIsBookmarkedPredicate>
        is_bookmarked_predicate)
    : page_url_predicate(page_url_predicate.Pass()),
      css_predicate(css_predicate.Pass()),
      is_bookmarked_predicate(is_bookmarked_predicate.Pass()) {
}

ContentCondition::~ContentCondition() {}

scoped_ptr<ContentCondition> CreateContentCondition(
    scoped_refptr<const Extension> extension,
    url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
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

  scoped_ptr<DeclarativeContentPageUrlPredicate> page_url_predicate;
  scoped_ptr<DeclarativeContentCssPredicate> css_predicate;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> is_bookmarked_predicate;

  for (base::DictionaryValue::Iterator iter(*condition_dict);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& predicate_name = iter.key();
    const base::Value& predicate_value = iter.value();
    if (predicate_name == keys::kInstanceType) {
      // Skip this.
    } else if (predicate_name == keys::kPageUrl) {
      page_url_predicate = CreatePageUrlPredicate(predicate_value,
                                                  url_matcher_condition_factory,
                                                  error);
    } else if (predicate_name == keys::kCss) {
      css_predicate = CreateCssPredicate(predicate_value, error);
    } else if (predicate_name == keys::kIsBookmarked){
      is_bookmarked_predicate =
          CreateIsBookmarkedPredicate(predicate_value, extension.get(), error);
    } else {
      *error = base::StringPrintf(kUnknownConditionAttribute,
                                  predicate_name.c_str());
    }
    if (!error->empty())
      return scoped_ptr<ContentCondition>();
  }

  return make_scoped_ptr(new ContentCondition(page_url_predicate.Pass(),
                                              css_predicate.Pass(),
                                              is_bookmarked_predicate.Pass()));
}

}  // namespace extensions
