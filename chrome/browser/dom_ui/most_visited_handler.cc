// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/most_visited_handler.h"

#include <set>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/md5.h"
#include "base/singleton.h"
#include "base/scoped_vector.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/dom_ui/web_ui_favicon_source.h"
#include "chrome/browser/dom_ui/web_ui_thumbnail_source.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// The number of most visited pages we show.
const size_t kMostVisitedPages = 8;

// The number of days of history we consider for most visited entries.
const int kMostVisitedScope = 90;

}  // namespace

// This struct is used when getting the pre-populated pages in case the user
// hasn't filled up his most visited pages.
struct MostVisitedHandler::MostVisitedPage {
  string16 title;
  GURL url;
  GURL thumbnail_url;
  GURL favicon_url;
};

MostVisitedHandler::MostVisitedHandler()
    : url_blacklist_(NULL),
      pinned_urls_(NULL),
      got_first_most_visited_request_(false) {
}

MostVisitedHandler::~MostVisitedHandler() {
}

WebUIMessageHandler* MostVisitedHandler::Attach(WebUI* web_ui) {
  Profile* profile = web_ui->GetProfile();
  url_blacklist_ = profile->GetPrefs()->GetMutableDictionary(
      prefs::kNTPMostVisitedURLsBlacklist);
  pinned_urls_ = profile->GetPrefs()->GetMutableDictionary(
      prefs::kNTPMostVisitedPinnedURLs);
  // Set up our sources for thumbnail and favicon data.
  WebUIThumbnailSource* thumbnail_src = new WebUIThumbnailSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(thumbnail_src);

  WebUIFavIconSource* favicon_src = new WebUIFavIconSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(favicon_src);

  // Get notifications when history is cleared.
  registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED,
                 Source<Profile>(profile));

  WebUIMessageHandler* result = WebUIMessageHandler::Attach(web_ui);

  // We pre-emptively make a fetch for the most visited pages so we have the
  // results sooner.
  StartQueryForMostVisited();
  return result;
}

void MostVisitedHandler::RegisterMessages() {
  // Register ourselves as the handler for the "mostvisited" message from
  // Javascript.
  dom_ui_->RegisterMessageCallback("getMostVisited",
      NewCallback(this, &MostVisitedHandler::HandleGetMostVisited));

  // Register ourselves for any most-visited item blacklisting.
  dom_ui_->RegisterMessageCallback("blacklistURLFromMostVisited",
      NewCallback(this, &MostVisitedHandler::HandleBlacklistURL));
  dom_ui_->RegisterMessageCallback("removeURLsFromMostVisitedBlacklist",
      NewCallback(this, &MostVisitedHandler::HandleRemoveURLsFromBlacklist));
  dom_ui_->RegisterMessageCallback("clearMostVisitedURLsBlacklist",
      NewCallback(this, &MostVisitedHandler::HandleClearBlacklist));

  // Register ourself for pinned URL messages.
  dom_ui_->RegisterMessageCallback("addPinnedURL",
      NewCallback(this, &MostVisitedHandler::HandleAddPinnedURL));
  dom_ui_->RegisterMessageCallback("removePinnedURL",
      NewCallback(this, &MostVisitedHandler::HandleRemovePinnedURL));
}

void MostVisitedHandler::HandleGetMostVisited(const ListValue* args) {
  if (!got_first_most_visited_request_) {
    // If our intial data is already here, return it.
    SendPagesValue();
    got_first_most_visited_request_ = true;
  } else {
    StartQueryForMostVisited();
  }
}

void MostVisitedHandler::SendPagesValue() {
  if (pages_value_.get()) {
    bool has_blacklisted_urls = !url_blacklist_->empty();
    history::TopSites* ts = dom_ui_->GetProfile()->GetTopSites();
    if (ts)
      has_blacklisted_urls = ts->HasBlacklistedItems();
    FundamentalValue first_run(IsFirstRun());
    FundamentalValue has_blacklisted_urls_value(has_blacklisted_urls);
    dom_ui_->CallJavascriptFunction(L"mostVisitedPages",
                                    *(pages_value_.get()),
                                    first_run,
                                    has_blacklisted_urls_value);
    pages_value_.reset();
  }
}

void MostVisitedHandler::StartQueryForMostVisited() {
  // Use TopSites.
  history::TopSites* ts = dom_ui_->GetProfile()->GetTopSites();
  if (ts) {
    ts->GetMostVisitedURLs(
        &topsites_consumer_,
        NewCallback(this, &MostVisitedHandler::OnMostVisitedURLsAvailable));
  }
}

void MostVisitedHandler::HandleBlacklistURL(const ListValue* args) {
  std::string url = WideToUTF8(ExtractStringValue(args));
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
    UserMetrics::RecordAction(UserMetricsAction("MostVisited_UrlRemoved"),
                              dom_ui_->GetProfile());
    history::TopSites* ts = dom_ui_->GetProfile()->GetTopSites();
    if (ts)
      ts->RemoveBlacklistedURL(GURL(url));
  }
}

void MostVisitedHandler::HandleClearBlacklist(const ListValue* args) {
  UserMetrics::RecordAction(UserMetricsAction("MostVisited_BlacklistCleared"),
                            dom_ui_->GetProfile());

  history::TopSites* ts = dom_ui_->GetProfile()->GetTopSites();
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
  history::TopSites* ts = dom_ui_->GetProfile()->GetTopSites();
  if (ts)
    ts->AddPinnedURL(page.url, index);
}

void MostVisitedHandler::HandleRemovePinnedURL(const ListValue* args) {
  std::string url = WideToUTF8(ExtractStringValue(args));
  RemovePinnedURL(GURL(url));
}

void MostVisitedHandler::RemovePinnedURL(const GURL& url) {
  history::TopSites* ts = dom_ui_->GetProfile()->GetTopSites();
  if (ts)
    ts->RemovePinnedURL(url);
}

bool MostVisitedHandler::GetPinnedURLAtIndex(int index,
                                             MostVisitedPage* page) {
  // This iterates over all the pinned URLs. It might seem like it is worth
  // having a map from the index to the item but the number of items is limited
  // to the number of items the most visited section is showing on the NTP so
  // this will be fast enough for now.
  for (DictionaryValue::key_iterator it = pinned_urls_->begin_keys();
      it != pinned_urls_->end_keys(); ++it) {
    Value* value;
    if (pinned_urls_->GetWithoutPathExpansion(*it, &value)) {
      if (!value->IsType(DictionaryValue::TYPE_DICTIONARY)) {
        // Moved on to TopSites and now going back.
        pinned_urls_->Clear();
        return false;
      }

      int dict_index;
      DictionaryValue* dict = static_cast<DictionaryValue*>(value);
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
    } else if (url.url.spec() ==
               l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_URL)) {
      page_value->SetString("thumbnailUrl",
          "chrome://theme/IDR_NEWTAB_THEMES_GALLERY_THUMBNAIL");
    }

    history::TopSites* ts = dom_ui_->GetProfile()->GetTopSites();
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

bool MostVisitedHandler::IsFirstRun() {
  // If we found no pages we treat this as the first run.
  bool first_run = NewTabUI::NewTabHTMLSource::first_run() &&
      pages_value_->GetSize() ==
          MostVisitedHandler::GetPrePopulatedPages().size();
  // but first_run should only be true once.
  NewTabUI::NewTabHTMLSource::set_first_run(false);
  return first_run;
}

// static
const std::vector<MostVisitedHandler::MostVisitedPage>&
    MostVisitedHandler::GetPrePopulatedPages() {
  // TODO(arv): This needs to get the data from some configurable place.
  // http://crbug.com/17630
  static std::vector<MostVisitedPage> pages;
  if (pages.empty()) {
    MostVisitedPage welcome_page = {
        l10n_util::GetStringUTF16(IDS_NEW_TAB_CHROME_WELCOME_PAGE_TITLE),
        GURL(l10n_util::GetStringUTF8(IDS_CHROME_WELCOME_URL)),
        GURL("chrome://theme/IDR_NEWTAB_CHROME_WELCOME_PAGE_THUMBNAIL"),
        GURL("chrome://theme/IDR_NEWTAB_CHROME_WELCOME_PAGE_FAVICON")};
    pages.push_back(welcome_page);

    MostVisitedPage gallery_page = {
        l10n_util::GetStringUTF16(IDS_NEW_TAB_THEMES_GALLERY_PAGE_TITLE),
        GURL(l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_URL)),
        GURL("chrome://theme/IDR_NEWTAB_THEMES_GALLERY_THUMBNAIL"),
        GURL("chrome://theme/IDR_NEWTAB_THEMES_GALLERY_FAVICON")};
    pages.push_back(gallery_page);
  }

  return pages;
}

void MostVisitedHandler::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type != NotificationType::HISTORY_URLS_DELETED) {
    NOTREACHED();
    return;
  }

  // Some URLs were deleted from history.  Reload the most visited list.
  HandleGetMostVisited(NULL);
}

void MostVisitedHandler::BlacklistURL(const GURL& url) {
  history::TopSites* ts = dom_ui_->GetProfile()->GetTopSites();
  if (ts)
    ts->AddBlacklistedURL(url);
}

std::string MostVisitedHandler::GetDictionaryKeyForURL(const std::string& url) {
  return MD5String(url);
}

// static
void MostVisitedHandler::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kNTPMostVisitedURLsBlacklist);
  prefs->RegisterDictionaryPref(prefs::kNTPMostVisitedPinnedURLs);
}

// static
std::vector<GURL> MostVisitedHandler::GetPrePopulatedUrls() {
  const std::vector<MostVisitedPage> pages =
      MostVisitedHandler::GetPrePopulatedPages();
  std::vector<GURL> page_urls;
  for (size_t i = 0; i < pages.size(); ++i)
    page_urls.push_back(pages[i].url);
  return page_urls;
}
