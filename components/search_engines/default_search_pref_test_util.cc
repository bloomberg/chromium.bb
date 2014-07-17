// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_pref_test_util.h"

#include "base/strings/string_split.h"
#include "components/search_engines/default_search_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

// static
scoped_ptr<base::DictionaryValue>
DefaultSearchPrefTestUtil::CreateDefaultSearchPreferenceValue(
    bool enabled,
    const std::string& name,
    const std::string& keyword,
    const std::string& search_url,
    const std::string& suggest_url,
    const std::string& icon_url,
    const std::string& encodings,
    const std::string& alternate_url,
    const std::string& search_terms_replacement_key) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  if (!enabled) {
    value->SetBoolean(DefaultSearchManager::kDisabledByPolicy, true);
    return value.Pass();
  }

  EXPECT_FALSE(keyword.empty());
  EXPECT_FALSE(search_url.empty());
  value->Set(DefaultSearchManager::kShortName,
             base::Value::CreateStringValue(name));
  value->Set(DefaultSearchManager::kKeyword,
             base::Value::CreateStringValue(keyword));
  value->Set(DefaultSearchManager::kURL,
             base::Value::CreateStringValue(search_url));
  value->Set(DefaultSearchManager::kSuggestionsURL,
             base::Value::CreateStringValue(suggest_url));
  value->Set(DefaultSearchManager::kFaviconURL,
             base::Value::CreateStringValue(icon_url));
  value->Set(DefaultSearchManager::kSearchTermsReplacementKey,
             base::Value::CreateStringValue(search_terms_replacement_key));

  std::vector<std::string> encodings_items;
  base::SplitString(encodings, ';', &encodings_items);
  scoped_ptr<base::ListValue> encodings_list(new base::ListValue);
  for (std::vector<std::string>::const_iterator it = encodings_items.begin();
       it != encodings_items.end();
       ++it) {
    encodings_list->AppendString(*it);
  }
  value->Set(DefaultSearchManager::kInputEncodings, encodings_list.release());

  scoped_ptr<base::ListValue> alternate_url_list(new base::ListValue());
  if (!alternate_url.empty())
    alternate_url_list->Append(base::Value::CreateStringValue(alternate_url));
  value->Set(DefaultSearchManager::kAlternateURLs,
             alternate_url_list.release());
  return value.Pass();
}
