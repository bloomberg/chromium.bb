// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/most_visited_handler.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_old.h"
#include "base/command_line.h"
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
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/user_metrics.h"
#include "content/common/notification_source.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

// This struct is used when getting the pre-populated pages in case the user
// hasn't filled up his most visited pages.
struct MostVisitedHandler::MostVisitedPage {
  string16 title;
  GURL url;
  GURL thumbnail_url;
  GURL favicon_url;
};

MostVisitedHandler::MostVisitedHandler()
    : got_first_most_visited_request_(false) {
}

MostVisitedHandler::~MostVisitedHandler() {
}

WebUIMessageHandler* MostVisitedHandler::Attach(WebUI* web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  // Set up our sources for thumbnail and favicon data.
  ThumbnailSource* thumbnail_src = new ThumbnailSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(thumbnail_src);

  profile->GetChromeURLDataManager()->AddDataSource(
      new FaviconSource(profile, FaviconSource::FAVICON));

  WebUIMessageHandler* result = WebUIMessageHandler::Attach(web_ui);

  history::TopSites* ts = profile->GetTopSites();
  if (ts) {
    // TopSites updates itself after a delay. This is especially noticable when
    // your profile is empty. Ask TopSites to update itself when we're about to
    // show the new tab page.
    ts->SyncWithHistory();

    // Register for notification when TopSites changes so that we can update
    // ourself.
    registrar_.Add(this, chrome::NOTIFICATION_TOP_SITES_CHANGED,
                   Source<history::TopSites>(ts));
  }

  // We pre-emptively make a fetch for the most visited pages so we have the
  // results sooner.
  StartQueryForMostVisited();
  return result;
}

void MostVisitedHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getMostVisited",
      base::Bind(&MostVisitedHandler::HandleGetMostVisited,
                 base::Unretained(this)));

  // Register ourselves for any most-visited item blacklisting.
  web_ui_->RegisterMessageCallback("blacklistURLFromMostVisited",
      base::Bind(&MostVisitedHandler::HandleBlacklistURL,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("removeURLsFromMostVisitedBlacklist",
      base::Bind(&MostVisitedHandler::HandleRemoveURLsFromBlacklist,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("clearMostVisitedURLsBlacklist",
      base::Bind(&MostVisitedHandler::HandleClearBlacklist,
                 base::Unretained(this)));

  // Register ourself for pinned URL messages.
  web_ui_->RegisterMessageCallback("addPinnedURL",
      base::Bind(&MostVisitedHandler::HandleAddPinnedURL,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("removePinnedURL",
      base::Bind(&MostVisitedHandler::HandleRemovePinnedURL,
                 base::Unretained(this)));
}

void MostVisitedHandler::HandleGetMostVisited(const ListValue* args) {
  if (!got_first_most_visited_request_) {
    // If our initial data is already here, return it.
    SendPagesValue();
    got_first_most_visited_request_ = true;
  } else {
    StartQueryForMostVisited();
  }
}

void MostVisitedHandler::SendPagesValue() {
  if (pages_value_.get()) {
    Profile* profile = Profile::FromWebUI(web_ui_);
    const DictionaryValue* url_blacklist =
        profile->GetPrefs()->GetDictionary(prefs::kNTPMostVisitedURLsBlacklist);
    bool has_blacklisted_urls = !url_blacklist->empty();
    history::TopSites* ts = profile->GetTopSites();
    if (ts)
      has_blacklisted_urls = ts->HasBlacklistedItems();
    base::FundamentalValue has_blacklisted_urls_value(has_blacklisted_urls);
    web_ui_->CallJavascriptFunction("setMostVisitedPages",
                                    *(pages_value_.get()),
                                    has_blacklisted_urls_value);
    pages_value_.reset();
  }
}

void MostVisitedHandler::StartQueryForMostVisited() {
  history::TopSites* ts = Profile::FromWebUI(web_ui_)->GetTopSites();
  if (ts) {
    ts->GetMostVisitedURLs(
        &topsites_consumer_,
        NewCallback(this, &MostVisitedHandler::OnMostVisitedURLsAvailable));
  }
}

void MostVisitedHandler::HandleBlacklistURL(const ListValue* args) {
  std::string url = UTF16ToUTF8(ExtractStringValue(args));
  BlacklistURL(GURL(url));
}

void MostVisitedHandler::HandleRemoveURLsFromBlacklist(const ListValue* args) {
  DCHECK(args->GetSize() != 0);

  for (ListValue::const_iterator iter = args->begin();
       iter != args->end(); ++iter) {
    std::string url;
    bool r = (*iter)->GetAsString(&url);
    if (!r) {
      NOTREACHED();
      return;
    }
    UserMetrics::RecordAction(UserMetricsAction("MostVisited_UrlRemoved"));
    history::TopSites* ts = Profile::FromWebUI(web_ui_)->GetTopSites();
    if (ts)
      ts->RemoveBlacklistedURL(GURL(url));
  }
}

void MostVisitedHandler::HandleClearBlacklist(const ListValue* args) {
  UserMetrics::RecordAction(UserMetricsAction("MostVisited_BlacklistCleared"));

  history::TopSites* ts = Profile::FromWebUI(web_ui_)->GetTopSites();
  if (ts)
    ts->ClearBlacklistedURLs();
}

void MostVisitedHandler::HandleAddPinnedURL(const ListValue* args) {
  DCHECK_EQ(5U, args->GetSize()) << "Wrong number of params to addPinnedURL";
  MostVisitedPage mvp;
  std::string tmp_string;
  string16 tmp_string16;
  int index;

  bool r = args->GetString(0, &tmp_string);
  DCHECK(r) << "Missing URL in addPinnedURL from the NTP Most Visited.";
  mvp.url = GURL(tmp_string);

  r = args->GetString(1, &tmp_string16);
  DCHECK(r) << "Missing title in addPinnedURL from the NTP Most Visited.";
  mvp.title = tmp_string16;

  r = args->GetString(2, &tmp_string);
  DCHECK(r) << "Failed to read the favicon URL in addPinnedURL from the NTP "
            << "Most Visited.";
  if (!tmp_string.empty())
    mvp.favicon_url = GURL(tmp_string);

  r = args->GetString(3, &tmp_string);
  DCHECK(r) << "Failed to read the thumbnail URL in addPinnedURL from the NTP "
            << "Most Visited.";
  if (!tmp_string.empty())
    mvp.thumbnail_url = GURL(tmp_string);

  r = args->GetString(4, &tmp_string);
  DCHECK(r) << "Missing index in addPinnedURL from the NTP Most Visited.";
  base::StringToInt(tmp_string, &index);

  AddPinnedURL(mvp, index);
}

void MostVisitedHandler::AddPinnedURL(const MostVisitedPage& page, int index) {
  history::TopSites* ts = Profile::FromWebUI(web_ui_)->GetTopSites();
  if (ts)
    ts->AddPinnedURL(page.url, index);
  UserMetrics::RecordAction(UserMetricsAction("MostVisited_UrlPinned"));
}

void MostVisitedHandler::HandleRemovePinnedURL(const ListValue* args) {
  std::string url = UTF16ToUTF8(ExtractStringValue(args));
  RemovePinnedURL(GURL(url));
}

void MostVisitedHandler::RemovePinnedURL(const GURL& url) {
  history::TopSites* ts = Profile::FromWebUI(web_ui_)->GetTopSites();
  if (ts)
    ts->RemovePinnedURL(url);
  UserMetrics::RecordAction(UserMetricsAction("MostVisited_UrlUnpinned"));
}

bool MostVisitedHandler::GetPinnedURLAtIndex(int index,
                                             MostVisitedPage* page) {
  // This iterates over all the pinned URLs. It might seem like it is worth
  // having a map from the index to the item but the number of items is limited
  // to the number of items the most visited section is showing on the NTP so
  // this will be fast enough for now.
  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  const DictionaryValue* pinned_urls =
      prefs->GetDictionary(prefs::kNTPMostVisitedPinnedURLs);
  for (DictionaryValue::key_iterator it = pinned_urls->begin_keys();
      it != pinned_urls->end_keys(); ++it) {
    Value* value;
    if (pinned_urls->GetWithoutPathExpansion(*it, &value)) {
      if (!value->IsType(DictionaryValue::TYPE_DICTIONARY)) {
        // Moved on to TopSites and now going back.
        DictionaryPrefUpdate update(prefs, prefs::kNTPMostVisitedPinnedURLs);
        update.Get()->Clear();
        return false;
      }

      int dict_index;
      const DictionaryValue* dict = static_cast<DictionaryValue*>(value);
      if (dict->GetInteger("index", &dict_index) && dict_index == index) {
        // The favicon and thumbnail URLs may be empty.
        std::string tmp_string;
        if (dict->GetString("faviconUrl", &tmp_string))
          page->favicon_url = GURL(tmp_string);
        if (dict->GetString("thumbnailUrl", &tmp_string))
          page->thumbnail_url = GURL(tmp_string);

        if (dict->GetString("url", &tmp_string))
          page->url = GURL(tmp_string);
        else
          return false;

        return dict->GetString("title", &page->title);
      }
    } else {
      NOTREACHED() << "DictionaryValue iterators are filthy liars.";
    }
  }

  return false;
}

void MostVisitedHandler::SetPagesValueFromTopSites(
    const history::MostVisitedURLList& data) {
  pages_value_.reset(new ListValue);
  for (size_t i = 0; i < data.size(); i++) {
    const history::MostVisitedURL& url = data[i];
    DictionaryValue* page_value = new DictionaryValue();
    if (url.url.is_empty()) {
      page_value->SetBoolean("filler", true);
      pages_value_->Append(page_value);
      continue;
    }

    NewTabUI::SetURLTitleAndDirection(page_value,
                                      url.title,
                                      url.url);
    if (!url.favicon_url.is_empty())
      page_value->SetString("faviconUrl", url.favicon_url.spec());

    // Special case for prepopulated pages: thumbnailUrl is different from url.
    if (url.url.spec() == l10n_util::GetStringUTF8(IDS_CHROME_WELCOME_URL)) {
      page_value->SetString("thumbnailUrl",
          "chrome://theme/IDR_NEWTAB_CHROME_WELCOME_PAGE_THUMBNAIL");
      page_value->SetString("faviconDominantColor", "rgb(0, 147, 60)");
    } else if (url.url.spec() ==
               l10n_util::GetStringUTF8(IDS_WEBSTORE_URL)) {
      page_value->SetString("thumbnailUrl",
          "chrome://theme/IDR_NEWTAB_WEBSTORE_THUMBNAIL");
      page_value->SetString("faviconDominantColor", "rgb(63, 132, 197)");
    }

    history::TopSites* ts = Profile::FromWebUI(web_ui_)->GetTopSites();
    if (ts && ts->IsURLPinned(url.url))
      page_value->SetBoolean("pinned", true);
    pages_value_->Append(page_value);
  }
}

void MostVisitedHandler::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& data) {
  SetPagesValueFromTopSites(data);
  if (got_first_most_visited_request_) {
    SendPagesValue();
  }
}

void MostVisitedHandler::Observe(int type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TOP_SITES_CHANGED);

  // Most visited urls changed, query again.
  StartQueryForMostVisited();
}

void MostVisitedHandler::BlacklistURL(const GURL& url) {
  history::TopSites* ts = Profile::FromWebUI(web_ui_)->GetTopSites();
  if (ts)
    ts->AddBlacklistedURL(url);
  UserMetrics::RecordAction(UserMetricsAction("MostVisited_UrlBlacklisted"));
}

std::string MostVisitedHandler::GetDictionaryKeyForURL(const std::string& url) {
  return base::MD5String(url);
}

// static
void MostVisitedHandler::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kNTPMostVisitedURLsBlacklist,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kNTPMostVisitedPinnedURLs,
                                PrefService::UNSYNCABLE_PREF);
}
