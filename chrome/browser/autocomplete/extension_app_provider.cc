// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/extension_app_provider.h"

#include <algorithm>
#include <cmath>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/profiles/profile.h"
#include "content/common/notification_service.h"
#include "ui/base/l10n/l10n_util.h"

ExtensionAppProvider::ExtensionAppProvider(ACProviderListener* listener,
                                           Profile* profile)
    : AutocompleteProvider(listener, profile, "ExtensionApps") {
  RegisterForNotifications();
  RefreshAppList();
}

void ExtensionAppProvider::AddExtensionAppForTesting(
    const std::string& app_name,
    const std::string url) {
  extension_apps_.push_back(std::make_pair(app_name, url));
}

void ExtensionAppProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  matches_.clear();

  if (input.type() == AutocompleteInput::INVALID)
    return;

  if (!input.text().empty()) {
    std::string input_utf8 = UTF16ToUTF8(input.text());
    for (ExtensionApps::const_iterator app = extension_apps_.begin();
         app != extension_apps_.end(); ++app) {
      // See if the input matches this extension application.
      const std::string& name = app->first;
      const std::string& url = app->second;
      std::string::const_iterator name_iter =
          std::search(name.begin(),
                      name.end(),
                      input_utf8.begin(),
                      input_utf8.end(),
                      base::CaseInsensitiveCompare<char>());
      std::string::const_iterator url_iter =
          std::search(url.begin(),
                      url.end(),
                      input_utf8.begin(),
                      input_utf8.end(),
                      base::CaseInsensitiveCompare<char>());

      bool matches_name = name_iter != name.end();
      bool matches_url = url_iter != url.end() &&
                         input.type() != AutocompleteInput::FORCED_QUERY;
      if (matches_name || matches_url) {
        // We have a match, might be a partial match.
        // TODO(finnur): Figure out what type to return here, might want to have
        // the extension icon/a generic icon show up in the Omnibox.
        AutocompleteMatch match(this, 0, false,
                                AutocompleteMatch::EXTENSION_APP);
        match.fill_into_edit = UTF8ToUTF16(url);
        match.destination_url = GURL(url);
        match.inline_autocomplete_offset = string16::npos;
        match.contents = UTF8ToUTF16(name);
        HighlightMatch(input, &match.contents_class, name_iter, name);
        match.description = UTF8ToUTF16(url);
        HighlightMatch(input, &match.description_class, url_iter, url);
        match.relevance = CalculateRelevance(input.type(),
                                             input.text().length(),
                                             matches_name ?
                                                 name.length() : url.length(),
                                             GURL(url));
        matches_.push_back(match);
      }
    }
  }
}

ExtensionAppProvider::~ExtensionAppProvider() {
}

void ExtensionAppProvider::RefreshAppList() {
  ExtensionService* extension_service = profile_->GetExtensionService();
  if (!extension_service)
    return;  // During testing, there is no extension service.
  const ExtensionList* extensions = extension_service->extensions();
  extension_apps_.clear();
  for (ExtensionList::const_iterator app = extensions->begin();
       app != extensions->end(); ++app) {
    if ((*app)->is_app() && (*app)->GetFullLaunchURL().is_valid()) {
      extension_apps_.push_back(
          std::make_pair((*app)->name(),
                         (*app)->GetFullLaunchURL().spec()));
    }
  }
}

void ExtensionAppProvider::RegisterForNotifications() {
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNINSTALLED,
                 NotificationService::AllSources());
}

void ExtensionAppProvider::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  RefreshAppList();
}

void ExtensionAppProvider::HighlightMatch(const AutocompleteInput& input,
                                          ACMatchClassifications* match_class,
                                          std::string::const_iterator iter,
                                          const std::string& match_string) {
  size_t pos = iter - match_string.begin();
  bool match_found = iter != match_string.end();
  if (!match_found || pos > 0) {
    match_class->push_back(
        ACMatchClassification(0, ACMatchClassification::DIM));
  }
  if (match_found) {
    match_class->push_back(
        ACMatchClassification(pos, ACMatchClassification::MATCH));
    if (pos + input.text().length() < match_string.length()) {
      match_class->push_back(ACMatchClassification(pos + input.text().length(),
                                ACMatchClassification::DIM));
    }
  }
}

int ExtensionAppProvider::CalculateRelevance(AutocompleteInput::Type type,
                                             int input_length,
                                             int target_length,
                                             const GURL& url) {
  // If you update the algorithm here, please remember to update the tables in
  // autocomplete.h also.
  const int kMaxRelevance = 1425;

  if (input_length == target_length)
    return kMaxRelevance;

  // We give a boost proportionally based on how much of the input matches the
  // app name, up to a maximum close to 200 (we can be close to, but we'll never
  // reach 200 because the 100% match is taken care of above).
  double fraction_boost = static_cast<double>(200) *
                          input_length / target_length;

  // We also give a boost relative to how often the user has previously typed
  // the Extension App URL/selected the Extension App suggestion from this
  // provider (boost is between 200-400).
  double type_count_boost = 0;
  HistoryService* const history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  history::URLDatabase* url_db = history_service ?
      history_service->InMemoryDatabase() : NULL;
  if (url_db) {
    history::URLRow info;
    url_db->GetRowForURL(url, &info);
    type_count_boost =
        400 * (1.0 - (std::pow(static_cast<double>(2), -info.typed_count())));
  }
  int relevance = 575 + static_cast<int>(type_count_boost) +
                        static_cast<int>(fraction_boost);
  DCHECK_LE(relevance, kMaxRelevance);
  return relevance;
}
