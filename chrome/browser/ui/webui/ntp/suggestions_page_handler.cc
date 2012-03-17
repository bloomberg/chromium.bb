// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/suggestions_page_handler.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/md5.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "googleurl/src/gurl.h"

using content::UserMetricsAction;

SuggestionsHandler::SuggestionsHandler()
    : got_first_suggestions_request_(false) {
}

SuggestionsHandler::~SuggestionsHandler() {
}

void SuggestionsHandler::RegisterMessages() {
  Profile* profile = Profile::FromWebUI(web_ui());
  // Set up our sources for thumbnail and favicon data.
  profile->GetChromeURLDataManager()->AddDataSource(
      new ThumbnailSource(profile));
  profile->GetChromeURLDataManager()->AddDataSource(
      new FaviconSource(profile, FaviconSource::FAVICON));

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

  // We pre-emptively make a fetch for the available pages so we have the
  // results sooner.
  StartQueryForSuggestions();

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
}

void SuggestionsHandler::HandleGetSuggestions(const ListValue* args) {
  if (!got_first_suggestions_request_) {
    // If our initial data is already here, return it.
    SendPagesValue();
    got_first_suggestions_request_ = true;
  } else {
    StartQueryForSuggestions();
  }
}

void SuggestionsHandler::SendPagesValue() {
  if (pages_value_.get()) {
    // TODO(georgey) add actual blacklist.
    bool has_blacklisted_urls = false;
    base::FundamentalValue has_blacklisted_urls_value(has_blacklisted_urls);
    web_ui()->CallJavascriptFunction("ntp.setSuggestionsPages",
                                     *(pages_value_.get()),
                                     has_blacklisted_urls_value);
    pages_value_.reset();
  }
}

void SuggestionsHandler::StartQueryForSuggestions() {
  // TODO(georgey) change it to provide our data.
  history::TopSites* top_sites = Profile::FromWebUI(web_ui())->GetTopSites();
  if (top_sites) {
    top_sites->GetMostVisitedURLs(
        &topsites_consumer_,
        base::Bind(&SuggestionsHandler::OnSuggestionsURLsAvailable,
                   base::Unretained(this)));
  }
}

void SuggestionsHandler::HandleBlacklistURL(const ListValue* args) {
  std::string url = UTF16ToUTF8(ExtractStringValue(args));
  BlacklistURL(GURL(url));
}

void SuggestionsHandler::HandleRemoveURLsFromBlacklist(const ListValue* args) {
  DCHECK_GT(args->GetSize(), 0U);
  // TODO(georgey) remove URLs from blacklist.
}

void SuggestionsHandler::HandleClearBlacklist(const ListValue* args) {
  // TODO(georgey) clear blacklist.
}

void SuggestionsHandler::SetPagesValueFromTopSites(
    const history::MostVisitedURLList& data) {
  // TODO(georgey) - change to our suggestions pages - right now as a test
  // returns top 20 sites in reverse order.
  pages_value_.reset(new ListValue());
  for (size_t i = 0; i < data.size(); i++) {
    const history::MostVisitedURL& suggested_url = data[data.size() - i - 1];
    if (suggested_url.url.is_empty())
      continue;

    DictionaryValue* page_value = new DictionaryValue();
    NewTabUI::SetURLTitleAndDirection(page_value,
                                      suggested_url.title,
                                      suggested_url.url);
    pages_value_->Append(page_value);
  }
}

void SuggestionsHandler::OnSuggestionsURLsAvailable(
    const history::MostVisitedURLList& data) {
  SetPagesValueFromTopSites(data);
  if (got_first_suggestions_request_)
    SendPagesValue();
}

void SuggestionsHandler::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TOP_SITES_CHANGED);

  // Suggestions urls changed, query again.
  StartQueryForSuggestions();
}

void SuggestionsHandler::BlacklistURL(const GURL& url) {
  // TODO(georgey) blacklist an URL.
}

std::string SuggestionsHandler::GetDictionaryKeyForURL(const std::string& url) {
  return base::MD5String(url);
}

// static
void SuggestionsHandler::RegisterUserPrefs(PrefService* prefs) {
  // TODO(georgey) add user preferences (such as own blacklist) as needed.
}
