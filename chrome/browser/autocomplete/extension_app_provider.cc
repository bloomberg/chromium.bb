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
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/l10n/l10n_util.h"

ExtensionAppProvider::ExtensionAppProvider(ACProviderListener* listener,
                                           Profile* profile)
    : AutocompleteProvider(listener, profile, "ExtensionApps") {
  RegisterForNotifications();
  RefreshAppList();
}

void ExtensionAppProvider::AddExtensionAppForTesting(const string16& app_name,
                                                     const string16& url) {
  extension_apps_.push_back(std::make_pair(app_name, url));
}

AutocompleteMatch ExtensionAppProvider::CreateAutocompleteMatch(
    const AutocompleteInput& input,
    const string16& name,
    const string16& url,
    size_t name_match_index,
    size_t url_match_index) {
  // TODO(finnur): Figure out what type to return here, might want to have
  // the extension icon/a generic icon show up in the Omnibox.
  AutocompleteMatch match(this, 0, false,
                          AutocompleteMatch::EXTENSION_APP);
  match.fill_into_edit = url;
  match.destination_url = GURL(url);
  match.inline_autocomplete_offset = string16::npos;
  match.contents = AutocompleteMatch::SanitizeString(name);
  AutocompleteMatch::ClassifyLocationInString(name_match_index,
      input.text().length(), name.length(), ACMatchClassification::NONE,
      &match.contents_class);
  match.description = url;
  AutocompleteMatch::ClassifyLocationInString(url_match_index,
      input.text().length(), url.length(), ACMatchClassification::URL,
      &match.description_class);
  match.relevance = CalculateRelevance(input.type(), input.text().length(),
      (name_match_index != string16::npos ? name.length() : url.length()),
      match.destination_url);
  return match;
}

void ExtensionAppProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  matches_.clear();

  if (input.type() == AutocompleteInput::INVALID)
    return;

  if (!input.text().empty()) {
    for (ExtensionApps::const_iterator app = extension_apps_.begin();
         app != extension_apps_.end(); ++app) {
      // See if the input matches this extension application.
      const string16& name = app->first;
      string16::const_iterator name_iter = std::search(name.begin(), name.end(),
          input.text().begin(), input.text().end(),
          base::CaseInsensitiveCompare<char16>());
      bool matches_name = name_iter != name.end();
      const string16& url = app->second;
      string16::const_iterator url_iter = std::search(url.begin(), url.end(),
          input.text().begin(), input.text().end(),
          base::CaseInsensitiveCompare<char16>());
      bool matches_url = url_iter != url.end() &&
                         input.type() != AutocompleteInput::FORCED_QUERY;

      if (matches_name || matches_url) {
        // We have a match, might be a partial match.
        matches_.push_back(CreateAutocompleteMatch(input, name, url,
            matches_name ?
                static_cast<size_t>(name_iter - name.begin()) : string16::npos,
            matches_url ?
                static_cast<size_t>(url_iter - url.begin()) : string16::npos));
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
      if (profile_->IsOffTheRecord() &&
          !extension_service->CanLoadInIncognito((*app)))
        continue;

      extension_apps_.push_back(
          std::make_pair(UTF8ToUTF16((*app)->name()),
                         UTF8ToUTF16((*app)->GetFullLaunchURL().spec())));
    }
  }
}

void ExtensionAppProvider::RegisterForNotifications() {
  // Notifications of extensions loading and unloading always come from the
  // non-incognito profile, but we need to see them regardless, as the incognito
  // windows can be affected.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
}

void ExtensionAppProvider::Observe(int type,
                                   const content::NotificationSource& source,
                                   const content::NotificationDetails& details) {
  RefreshAppList();
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
