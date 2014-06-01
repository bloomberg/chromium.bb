// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/managed_bookmarks_policy_handler.h"

#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "chrome/common/net/url_fixer_upper.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"
#include "url/gurl.h"

namespace policy {

const char ManagedBookmarksPolicyHandler::kName[] = "name";
const char ManagedBookmarksPolicyHandler::kUrl[] = "url";
const char ManagedBookmarksPolicyHandler::kChildren[] = "children";

ManagedBookmarksPolicyHandler::ManagedBookmarksPolicyHandler(
    Schema chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kManagedBookmarks,
          chrome_schema.GetKnownProperty(key::kManagedBookmarks),
          SCHEMA_ALLOW_INVALID) {}

ManagedBookmarksPolicyHandler::~ManagedBookmarksPolicyHandler() {}

void ManagedBookmarksPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  scoped_ptr<base::Value> value;
  if (!CheckAndGetValue(policies, NULL, &value))
    return;

  base::ListValue* list = NULL;
  if (!value || !value->GetAsList(&list))
    return;

  FilterBookmarks(list);
  prefs->SetValue(prefs::kManagedBookmarks, value.release());
}

void ManagedBookmarksPolicyHandler::FilterBookmarks(base::ListValue* list) {
  // Remove any non-conforming values found.
  base::ListValue::iterator it = list->begin();
  while (it != list->end()) {
    base::DictionaryValue* dict = NULL;
    if (!*it || !(*it)->GetAsDictionary(&dict)) {
      it = list->Erase(it, NULL);
      continue;
    }

    std::string name;
    std::string url;
    base::ListValue* children = NULL;
    // Every bookmark must have a name, and then either a URL of a list of
    // child bookmarks.
    if (!dict->GetString(kName, &name) ||
        (!dict->GetList(kChildren, &children) &&
         !dict->GetString(kUrl, &url))) {
      it = list->Erase(it, NULL);
      continue;
    }

    if (children) {
      // Ignore the URL if this bookmark has child nodes.
      dict->Remove(kUrl, NULL);
      FilterBookmarks(children);
    } else {
      // Make sure the URL is valid before passing a bookmark to the pref.
      dict->Remove(kChildren, NULL);
      GURL gurl = URLFixerUpper::FixupURL(url, "");
      if (!gurl.is_valid()) {
        it = list->Erase(it, NULL);
        continue;
      }
      dict->SetString(kUrl, gurl.spec());
    }

    ++it;
  }
}

}  // namespace policy
