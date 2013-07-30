// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler_android.h"

#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/common/net/url_fixer_upper.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"
#include "url/gurl.h"

namespace policy {

namespace {

bool GetBookmark(const base::Value& value,
                 std::string* name,
                 std::string* url) {
  const base::DictionaryValue* dict = NULL;
  if (!value.GetAsDictionary(&dict))
    return false;
  std::string url_string;
  if (!dict->GetStringWithoutPathExpansion(ManagedBookmarksPolicyHandler::kName,
                                           name) ||
      !dict->GetStringWithoutPathExpansion(ManagedBookmarksPolicyHandler::kUrl,
                                           &url_string)) {
    return false;
  }
  GURL gurl = URLFixerUpper::FixupURL(url_string, "");
  if (!gurl.is_valid())
    return false;
  *url = gurl.spec();
  return true;
}

}  // namespace

const char ManagedBookmarksPolicyHandler::kName[] = "name";
const char ManagedBookmarksPolicyHandler::kUrl[] = "url";

ManagedBookmarksPolicyHandler::ManagedBookmarksPolicyHandler()
    : TypeCheckingPolicyHandler(key::kManagedBookmarks,
                                base::Value::TYPE_LIST) {}

ManagedBookmarksPolicyHandler::~ManagedBookmarksPolicyHandler() {}

bool ManagedBookmarksPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value = NULL;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (!value)
    return true;

  const base::ListValue* list = NULL;
  value->GetAsList(&list);
  DCHECK(list);

  for (base::ListValue::const_iterator it = list->begin();
       it != list->end(); ++it) {
    std::string name;
    std::string url;
    if (!*it || !GetBookmark(**it, &name, &url)) {
      size_t index = it - list->begin();
      errors->AddError(policy_name(), IDS_POLICY_INVALID_BOOKMARK, index);
    }
  }

  return true;
}

void ManagedBookmarksPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  const base::ListValue* list = NULL;
  if (!value || !value->GetAsList(&list))
    return;

  base::ListValue* bookmarks = new base::ListValue();
  for (base::ListValue::const_iterator it = list->begin();
       it != list->end(); ++it) {
    std::string name;
    std::string url;
    if (*it && GetBookmark(**it, &name, &url)) {
      base::DictionaryValue* dict = new base::DictionaryValue();
      dict->SetString(kName, name);
      dict->SetString(kUrl, url);
      bookmarks->Append(dict);
    }
  }

  prefs->SetValue(prefs::kManagedBookmarks, bookmarks);
}

}  // namespace policy
