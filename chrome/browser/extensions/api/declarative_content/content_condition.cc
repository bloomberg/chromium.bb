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

  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set;
  std::vector<std::string> css_rules;
  // Possible states for matching bookmarked state.
  enum BookmarkedStateMatch { NOT_BOOKMARKED, BOOKMARKED, DONT_CARE };
  BookmarkedStateMatch bookmarked_state = DONT_CARE;

  for (base::DictionaryValue::Iterator iter(*condition_dict);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& condition_attribute_name = iter.key();
    const base::Value& condition_attribute_value = iter.value();
    if (condition_attribute_name == keys::kInstanceType) {
      // Skip this.
    } else if (condition_attribute_name == keys::kPageUrl) {
      const base::DictionaryValue* dict = NULL;
      if (!condition_attribute_value.GetAsDictionary(&dict)) {
        *error = base::StringPrintf(kInvalidTypeOfParameter,
                                    condition_attribute_name.c_str());
      } else {
        url_matcher_condition_set =
            url_matcher::URLMatcherFactory::CreateFromURLFilterDictionary(
                url_matcher_condition_factory, dict, ++g_next_id, error);
      }
    } else if (condition_attribute_name == keys::kCss) {
      const base::ListValue* css_rules_value = NULL;
      if (condition_attribute_value.GetAsList(&css_rules_value)) {
        for (size_t i = 0; i < css_rules_value->GetSize(); ++i) {
          std::string css_rule;
          if (!css_rules_value->GetString(i, &css_rule)) {
            *error = base::StringPrintf(kInvalidTypeOfParameter,
                                        condition_attribute_name.c_str());
            break;
          }
          css_rules.push_back(css_rule);
        }
      } else {
        *error = base::StringPrintf(kInvalidTypeOfParameter,
                                    condition_attribute_name.c_str());
      }
    } else if (condition_attribute_name == keys::kIsBookmarked){
      bool value;
      if (condition_attribute_value.GetAsBoolean(&value)) {
        if (!HasBookmarkAPIPermission(extension.get()))
          *error = kIsBookmarkedRequiresBookmarkPermission;
        else
          bookmarked_state = value ? BOOKMARKED : NOT_BOOKMARKED;
      } else {
        *error = base::StringPrintf(kInvalidTypeOfParameter,
                                    condition_attribute_name.c_str());
      }
    } else {
      *error = base::StringPrintf(kUnknownConditionAttribute,
                                  condition_attribute_name.c_str());
    }
    if (!error->empty())
      return scoped_ptr<ContentCondition>();
  }

  scoped_ptr<DeclarativeContentPageUrlPredicate> page_url_predicate;
  scoped_ptr<DeclarativeContentCssPredicate> css_predicate;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> is_bookmarked_predicate;

  if (url_matcher_condition_set) {
    page_url_predicate.reset(
        new DeclarativeContentPageUrlPredicate(url_matcher_condition_set));
  }
  if (!css_rules.empty())
    css_predicate.reset(new DeclarativeContentCssPredicate(css_rules));
  if (bookmarked_state != DONT_CARE) {
    is_bookmarked_predicate.reset(
        new DeclarativeContentIsBookmarkedPredicate(
            extension,
            bookmarked_state == BOOKMARKED));
  }

  return make_scoped_ptr(new ContentCondition(page_url_predicate.Pass(),
                                              css_predicate.Pass(),
                                              is_bookmarked_predicate.Pass()));
}

}  // namespace extensions
