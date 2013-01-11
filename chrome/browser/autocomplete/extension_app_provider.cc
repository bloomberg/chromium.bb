// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/extension_app_provider.h"

#include <algorithm>
#include <cmath>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/l10n/l10n_util.h"

ExtensionAppProvider::ExtensionAppProvider(
    AutocompleteProviderListener* listener,
    Profile* profile)
    : AutocompleteProvider(listener, profile,
          AutocompleteProvider::TYPE_EXTENSION_APP) {
  // Notifications of extensions loading and unloading always come from the
  // non-incognito profile, but we need to see them regardless, as the incognito
  // windows can be affected.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  RefreshAppList();
}

// static.
void ExtensionAppProvider::LaunchAppFromOmnibox(
    const AutocompleteMatch& match,
    Profile* profile,
    WindowOpenDisposition disposition) {
  ExtensionService* service =
      extensions::ExtensionSystemFactory::GetForProfile(profile)->
      extension_service();
  const extensions::Extension* extension =
      service->GetInstalledApp(match.destination_url);
  // While the Omnibox popup is open, the extension can be updated, changing
  // its URL and leaving us with no extension being found. In this case, we
  // ignore the request.
  if (!extension)
    return;

  AppLauncherHandler::RecordAppLaunchType(
      extension_misc::APP_LAUNCH_OMNIBOX_APP);

  application_launch::OpenApplication(application_launch::LaunchParams(
          profile, extension, disposition));
}

void ExtensionAppProvider::AddExtensionAppForTesting(
    const ExtensionApp& extension_app) {
  extension_apps_.push_back(extension_app);
}

AutocompleteMatch ExtensionAppProvider::CreateAutocompleteMatch(
    const AutocompleteInput& input,
    const ExtensionApp& app,
    size_t name_match_index,
    size_t url_match_index) {
  // TODO(finnur): Figure out what type to return here, might want to have
  // the extension icon/a generic icon show up in the Omnibox.
  AutocompleteMatch match(this, 0, false, AutocompleteMatch::EXTENSION_APP);
  match.fill_into_edit =
      app.should_match_against_launch_url ? app.launch_url : input.text();
  match.destination_url = GURL(app.launch_url);
  match.inline_autocomplete_offset = string16::npos;
  match.contents = AutocompleteMatch::SanitizeString(app.name);
  AutocompleteMatch::ClassifyLocationInString(name_match_index,
      input.text().length(), app.name.length(), ACMatchClassification::NONE,
      &match.contents_class);
  if (app.should_match_against_launch_url) {
    match.description = app.launch_url;
    AutocompleteMatch::ClassifyLocationInString(url_match_index,
        input.text().length(), app.launch_url.length(),
        ACMatchClassification::URL, &match.description_class);
  }
  match.relevance = CalculateRelevance(
      input.type(),
      input.text().length(),
      name_match_index != string16::npos ?
          app.name.length() : app.launch_url.length(),
      match.destination_url);
  return match;
}

void ExtensionAppProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  matches_.clear();

  if ((input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY))
    return;

  if (input.text().empty())
    return;

  for (ExtensionApps::const_iterator app = extension_apps_.begin();
       app != extension_apps_.end(); ++app) {
    // See if the input matches this extension application.
    const string16& name = app->name;
    string16::const_iterator name_iter = std::search(name.begin(), name.end(),
        input.text().begin(), input.text().end(),
        base::CaseInsensitiveCompare<char16>());
    bool matches_name = name_iter != name.end();
    size_t name_match_index = matches_name ?
        static_cast<size_t>(name_iter - name.begin()) : string16::npos;

    bool matches_url = false;
    size_t url_match_index = string16::npos;
    if (app->should_match_against_launch_url) {
      const string16& url = app->launch_url;
      string16::const_iterator url_iter = std::search(url.begin(), url.end(),
          input.text().begin(), input.text().end(),
          base::CaseInsensitiveCompare<char16>());
      matches_url = url_iter != url.end() &&
          input.type() != AutocompleteInput::FORCED_QUERY;
      url_match_index = matches_url ?
          static_cast<size_t>(url_iter - url.begin()) : string16::npos;
    }

    if (matches_name || matches_url) {
      // We have a match, might be a partial match.
      matches_.push_back(CreateAutocompleteMatch(
          input, *app, name_match_index, url_match_index));
    }
  }
}

ExtensionAppProvider::~ExtensionAppProvider() {
}

void ExtensionAppProvider::RefreshAppList() {
  ExtensionService* extension_service =
      extensions::ExtensionSystemFactory::GetForProfile(profile_)->
      extension_service();
  if (!extension_service)
    return;  // During testing, there is no extension service.
  const ExtensionSet* extensions = extension_service->extensions();
  extension_apps_.clear();
  for (ExtensionSet::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    const extensions::Extension* app = *iter;
    if (!app->ShouldDisplayInAppLauncher())
      continue;
    // Note: Apps that appear in the NTP only are not added here since this
    // provider is currently only used in the app launcher.

    if (profile_->IsOffTheRecord() &&
        !extension_service->CanLoadInIncognito(app))
      continue;

    GURL launch_url =
        app->is_platform_app() ? app->url() : app->GetFullLaunchURL();
    DCHECK(launch_url.is_valid());

    ExtensionApp extension_app = {
        UTF8ToUTF16(app->name()),
        UTF8ToUTF16(launch_url.spec()),
        // Only hosted apps have recognizable URLs that users might type in,
        // packaged apps and hosted apps use chrome-extension:// URLs that are
        // normally not shown to users.
        app->is_hosted_app()
    };
    extension_apps_.push_back(extension_app);
  }
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
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
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
