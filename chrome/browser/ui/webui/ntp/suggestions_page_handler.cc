// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/suggestions_page_handler.h"

#include <math.h>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/md5.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/ntp_stats.h"
#include "chrome/browser/ui/webui/ntp/suggestions_combiner.h"
#include "chrome/browser/ui/webui/ntp/suggestions_source_top_sites.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/common/url_constants.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/page_transition_types.h"
#include "url/gurl.h"

using base::UserMetricsAction;

SuggestionsHandler::SuggestionsHandler()
    : got_first_suggestions_request_(false),
      suggestions_viewed_(false),
      user_action_logged_(false) {
}

SuggestionsHandler::~SuggestionsHandler() {
  if (!user_action_logged_ && suggestions_viewed_) {
    const GURL ntp_url = GURL(chrome::kChromeUINewTabURL);
    int action_id = NTP_FOLLOW_ACTION_OTHER;
    content::NavigationEntry* entry =
        web_ui()->GetWebContents()->GetController().GetLastCommittedEntry();
    if (entry && (entry->GetURL() != ntp_url)) {
      action_id =
          content::PageTransitionStripQualifier(entry->GetTransitionType());
    }

    UMA_HISTOGRAM_ENUMERATION("NewTabPage.SuggestedSitesAction", action_id,
                              NUM_NTP_FOLLOW_ACTIONS);
  }
}

void SuggestionsHandler::RegisterMessages() {
  Profile* profile = Profile::FromWebUI(web_ui());
  // Set up our sources for thumbnail and favicon data.
  content::URLDataSource::Add(profile, new ThumbnailSource(profile, false));
  content::URLDataSource::Add(
      profile, new FaviconSource(profile, FaviconSource::FAVICON));

  // TODO(georgey) change the source of the web-sites to provide our data.
  // Initial commit uses top sites as a data source.
  history::TopSites* top_sites = profile->GetTopSites();
  if (top_sites) {
    // TopSites updates itself after a delay. This is especially noticable when
    // your profile is empty. Ask TopSites to update itself when we're about to
    // show the new tab page.
    top_sites->SyncWithHistory();

    // Register for notification when TopSites changes so that we can update
    // ourself.
    registrar_.Add(this, chrome::NOTIFICATION_TOP_SITES_CHANGED,
                   content::Source<history::TopSites>(top_sites));
  }

  // Setup the suggestions sources.
  SuggestionsCombiner* combiner = new SuggestionsCombiner(this, profile);
  combiner->AddSource(new SuggestionsSourceTopSites());
  suggestions_combiner_.reset(combiner);

  // We pre-emptively make a fetch for suggestions so we have the results
  // sooner.
  suggestions_combiner_->FetchItems(profile);

  web_ui()->RegisterMessageCallback("getSuggestions",
      base::Bind(&SuggestionsHandler::HandleGetSuggestions,
                 base::Unretained(this)));
  // Register ourselves for any suggestions item blacklisting.
  web_ui()->RegisterMessageCallback("blacklistURLFromSuggestions",
      base::Bind(&SuggestionsHandler::HandleBlacklistURL,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeURLsFromSuggestionsBlacklist",
      base::Bind(&SuggestionsHandler::HandleRemoveURLsFromBlacklist,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("clearSuggestionsURLsBlacklist",
      base::Bind(&SuggestionsHandler::HandleClearBlacklist,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("suggestedSitesAction",
      base::Bind(&SuggestionsHandler::HandleSuggestedSitesAction,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("suggestedSitesSelected",
      base::Bind(&SuggestionsHandler::HandleSuggestedSitesSelected,
                 base::Unretained(this)));
}

void SuggestionsHandler::HandleGetSuggestions(const base::ListValue* args) {
  if (!got_first_suggestions_request_) {
    // If it's the first request we get, return the prefetched data.
    SendPagesValue();
    got_first_suggestions_request_ = true;
  } else {
    suggestions_combiner_->FetchItems(Profile::FromWebUI(web_ui()));
  }
}

void SuggestionsHandler::OnSuggestionsReady() {
  // If we got the results as a result of a suggestions request initiated by the
  // JavaScript then we send back the page values.
  if (got_first_suggestions_request_)
    SendPagesValue();
}

void SuggestionsHandler::SendPagesValue() {
  if (suggestions_combiner_->GetPageValues()) {
    // TODO(georgey) add actual blacklist.
    bool has_blacklisted_urls = false;
    base::FundamentalValue has_blacklisted_urls_value(has_blacklisted_urls);
    web_ui()->CallJavascriptFunction("ntp.setSuggestionsPages",
                                     *suggestions_combiner_->GetPageValues(),
                                     has_blacklisted_urls_value);
  }
}

void SuggestionsHandler::HandleBlacklistURL(const base::ListValue* args) {
  std::string url = base::UTF16ToUTF8(ExtractStringValue(args));
  BlacklistURL(GURL(url));
}

void SuggestionsHandler::HandleRemoveURLsFromBlacklist(
    const base::ListValue* args) {
  DCHECK_GT(args->GetSize(), 0U);
  // TODO(georgey) remove URLs from blacklist.
}

void SuggestionsHandler::HandleClearBlacklist(const base::ListValue* args) {
  // TODO(georgey) clear blacklist.
}

void SuggestionsHandler::HandleSuggestedSitesAction(
    const base::ListValue* args) {
  DCHECK(args);

  double action_id;
  if (!args->GetDouble(0, &action_id))
    NOTREACHED();

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.SuggestedSitesAction",
                            static_cast<int>(action_id),
                            NUM_NTP_FOLLOW_ACTIONS);
  suggestions_viewed_ = true;
  user_action_logged_ = true;
}

void SuggestionsHandler::HandleSuggestedSitesSelected(
    const base::ListValue* args) {
  suggestions_viewed_ = true;
}

void SuggestionsHandler::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TOP_SITES_CHANGED);

  // Suggestions urls changed, query again.
  suggestions_combiner_->FetchItems(Profile::FromWebUI(web_ui()));
}

void SuggestionsHandler::BlacklistURL(const GURL& url) {
  // TODO(georgey) blacklist an URL.
}

std::string SuggestionsHandler::GetDictionaryKeyForURL(const std::string& url) {
  return base::MD5String(url);
}

// static
void SuggestionsHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(georgey) add user preferences (such as own blacklist) as needed.
}
