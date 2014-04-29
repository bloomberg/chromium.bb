// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_DEFAULT_SEARCH_MANAGER_H_
#define CHROME_BROWSER_SEARCH_ENGINES_DEFAULT_SEARCH_MANAGER_H_

#include "base/macros.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;
struct TemplateURLData;

// DefaultSearchManager handles the loading and writing of the user's default
// search engine selection to and from prefs.

class DefaultSearchManager {
 public:
  static const char kID[];
  static const char kShortName[];
  static const char kKeyword[];
  static const char kPrepopulateID[];
  static const char kSyncGUID[];

  static const char kURL[];
  static const char kSuggestionsURL[];
  static const char kInstantURL[];
  static const char kImageURL[];
  static const char kNewTabURL[];
  static const char kFaviconURL[];
  static const char kOriginatingURL[];

  static const char kSearchURLPostParams[];
  static const char kSuggestionsURLPostParams[];
  static const char kInstantURLPostParams[];
  static const char kImageURLPostParams[];

  static const char kSafeForAutoReplace[];
  static const char kInputEncodings[];

  static const char kDateCreated[];
  static const char kLastModified[];

  static const char kUsageCount[];
  static const char kAlternateURLs[];
  static const char kSearchTermsReplacementKey[];
  static const char kCreatedByPolicy[];

  explicit DefaultSearchManager(PrefService* pref_service);
  ~DefaultSearchManager();

  // Register prefs needed for tracking the default search provider.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Read default search provider data from |pref_service_|.
  bool GetDefaultSearchEngine(TemplateURLData* url);

  // Write default search provider data to |pref_service_|.
  void SetUserSelectedDefaultSearchEngine(const TemplateURLData& data);

  // Clear the user's default search provider choice from |pref_service_|.
  void ClearUserSelectedDefaultSearchEngine();

 private:
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchManager);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_DEFAULT_SEARCH_MANAGER_H_
