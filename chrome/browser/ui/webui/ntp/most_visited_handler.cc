// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/most_visited_handler.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/md5.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_utils.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/ntp_stats.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/common/pref_names.h"
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
#include "url/gurl.h"

using base::UserMetricsAction;

MostVisitedHandler::MostVisitedHandler()
    : got_first_most_visited_request_(false),
      most_visited_viewed_(false),
      user_action_logged_(false),
      weak_ptr_factory_(this) {
}

MostVisitedHandler::~MostVisitedHandler() {
  if (!user_action_logged_ && most_visited_viewed_) {
    const GURL ntp_url = GURL(chrome::kChromeUINewTabURL);
    int action_id = NTP_FOLLOW_ACTION_OTHER;
    content::NavigationEntry* entry =
        web_ui()->GetWebContents()->GetController().GetLastCommittedEntry();
    if (entry && (entry->GetURL() != ntp_url)) {
      action_id =
          ui::PageTransitionStripQualifier(entry->GetTransitionType());
    }

    UMA_HISTOGRAM_ENUMERATION("NewTabPage.MostVisitedAction", action_id,
                              NUM_NTP_FOLLOW_ACTIONS);
  }
}

void MostVisitedHandler::RegisterMessages() {
  Profile* profile = Profile::FromWebUI(web_ui());
  // Set up our sources for thumbnail and favicon data.
  content::URLDataSource::Add(profile, new ThumbnailSource(profile, false));
  content::URLDataSource::Add(profile, new ThumbnailSource(profile, true));

  // Set up our sources for top-sites data.
  content::URLDataSource::Add(profile, new ThumbnailListSource(profile));

  // Register chrome://favicon as a data source for favicons.
  content::URLDataSource::Add(
      profile, new FaviconSource(profile, FaviconSource::FAVICON));

  history::TopSites* ts = profile->GetTopSites();
  if (ts) {
    // TopSites updates itself after a delay. This is especially noticable when
    // your profile is empty. Ask TopSites to update itself when we're about to
    // show the new tab page.
    ts->SyncWithHistory();

    // Register for notification when TopSites changes so that we can update
    // ourself.
    registrar_.Add(this, chrome::NOTIFICATION_TOP_SITES_CHANGED,
                   content::Source<history::TopSites>(ts));
  }

  // We pre-emptively make a fetch for the most visited pages so we have the
  // results sooner.
  StartQueryForMostVisited();

  web_ui()->RegisterMessageCallback("getMostVisited",
      base::Bind(&MostVisitedHandler::HandleGetMostVisited,
                 base::Unretained(this)));

  // Register ourselves for any most-visited item blacklisting.
  web_ui()->RegisterMessageCallback("blacklistURLFromMostVisited",
      base::Bind(&MostVisitedHandler::HandleBlacklistUrl,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeURLsFromMostVisitedBlacklist",
      base::Bind(&MostVisitedHandler::HandleRemoveUrlsFromBlacklist,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("clearMostVisitedURLsBlacklist",
      base::Bind(&MostVisitedHandler::HandleClearBlacklist,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("mostVisitedAction",
      base::Bind(&MostVisitedHandler::HandleMostVisitedAction,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("mostVisitedSelected",
      base::Bind(&MostVisitedHandler::HandleMostVisitedSelected,
                 base::Unretained(this)));
}

void MostVisitedHandler::HandleGetMostVisited(const base::ListValue* args) {
  if (!got_first_most_visited_request_) {
    // If our initial data is already here, return it.
    SendPagesValue();
    got_first_most_visited_request_ = true;
  } else {
    StartQueryForMostVisited();
  }
}

void MostVisitedHandler::SendPagesValue() {
  if (pages_value_) {
    Profile* profile = Profile::FromWebUI(web_ui());
    const base::DictionaryValue* url_blacklist =
        profile->GetPrefs()->GetDictionary(prefs::kNtpMostVisitedURLsBlacklist);
    bool has_blacklisted_urls = !url_blacklist->empty();
    history::TopSites* ts = profile->GetTopSites();
    if (ts)
      has_blacklisted_urls = ts->HasBlacklistedItems();

    base::FundamentalValue has_blacklisted_urls_value(has_blacklisted_urls);
    web_ui()->CallJavascriptFunction("ntp.setMostVisitedPages",
                                     *pages_value_,
                                     has_blacklisted_urls_value);
    pages_value_.reset();
  }
}

void MostVisitedHandler::StartQueryForMostVisited() {
  history::TopSites* ts = Profile::FromWebUI(web_ui())->GetTopSites();
  if (ts) {
    ts->GetMostVisitedURLs(
        base::Bind(&MostVisitedHandler::OnMostVisitedUrlsAvailable,
                   weak_ptr_factory_.GetWeakPtr()), false);
  }
}

void MostVisitedHandler::HandleBlacklistUrl(const base::ListValue* args) {
  std::string url = base::UTF16ToUTF8(ExtractStringValue(args));
  BlacklistUrl(GURL(url));
}

void MostVisitedHandler::HandleRemoveUrlsFromBlacklist(
    const base::ListValue* args) {
  DCHECK(args->GetSize() != 0);

  for (base::ListValue::const_iterator iter = args->begin();
       iter != args->end(); ++iter) {
    std::string url;
    bool r = (*iter)->GetAsString(&url);
    if (!r) {
      NOTREACHED();
      return;
    }
    content::RecordAction(UserMetricsAction("MostVisited_UrlRemoved"));
    history::TopSites* ts = Profile::FromWebUI(web_ui())->GetTopSites();
    if (ts)
      ts->RemoveBlacklistedURL(GURL(url));
  }
}

void MostVisitedHandler::HandleClearBlacklist(const base::ListValue* args) {
  content::RecordAction(UserMetricsAction("MostVisited_BlacklistCleared"));

  history::TopSites* ts = Profile::FromWebUI(web_ui())->GetTopSites();
  if (ts)
    ts->ClearBlacklistedURLs();
}

void MostVisitedHandler::HandleMostVisitedAction(const base::ListValue* args) {
  DCHECK(args);

  double action_id;
  if (!args->GetDouble(0, &action_id))
    NOTREACHED();

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.MostVisitedAction",
                            static_cast<int>(action_id),
                            NUM_NTP_FOLLOW_ACTIONS);
  most_visited_viewed_ = true;
  user_action_logged_ = true;
}

void MostVisitedHandler::HandleMostVisitedSelected(
    const base::ListValue* args) {
  most_visited_viewed_ = true;
}

void MostVisitedHandler::SetPagesValueFromTopSites(
    const history::MostVisitedURLList& data) {
  pages_value_.reset(new base::ListValue);

  history::MostVisitedURLList top_sites(data);
  for (size_t i = 0; i < top_sites.size(); i++) {
    const history::MostVisitedURL& url = top_sites[i];
    base::DictionaryValue* page_value = new base::DictionaryValue();
    if (url.url.is_empty()) {
      page_value->SetBoolean("filler", true);
      pages_value_->Append(page_value);
      continue;
    }

    NewTabUI::SetUrlTitleAndDirection(page_value,
                                      url.title,
                                      url.url);
    pages_value_->Append(page_value);
  }
}

void MostVisitedHandler::OnMostVisitedUrlsAvailable(
    const history::MostVisitedURLList& data) {
  SetPagesValueFromTopSites(data);
  if (got_first_most_visited_request_) {
    SendPagesValue();
  }
}

void MostVisitedHandler::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TOP_SITES_CHANGED);

  // Most visited urls changed, query again.
  StartQueryForMostVisited();
}

void MostVisitedHandler::BlacklistUrl(const GURL& url) {
  history::TopSites* ts = Profile::FromWebUI(web_ui())->GetTopSites();
  if (ts)
    ts->AddBlacklistedURL(url);
  content::RecordAction(UserMetricsAction("MostVisited_UrlBlacklisted"));
}

std::string MostVisitedHandler::GetDictionaryKeyForUrl(const std::string& url) {
  return base::MD5String(url);
}

// static
void MostVisitedHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kNtpMostVisitedURLsBlacklist,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}
