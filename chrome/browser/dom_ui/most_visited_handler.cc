// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/most_visited_handler.h"

#include "app/l10n_util.h"
#include "base/md5.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui_favicon_source.h"
#include "chrome/browser/dom_ui/dom_ui_thumbnail_source.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// The number of most visited pages we show.
const size_t kMostVisitedPages = 8;

// The number of days of history we consider for most visited entries.
const int kMostVisitedScope = 90;

// Adds the fields in the page to the dictionary.
void SetMostVisistedPage(DictionaryValue* dict,
                         const MostVisitedHandler::MostVisitedPage& page) {
  NewTabUI::SetURLTitleAndDirection(dict, WideToUTF16(page.title), page.url);
  if (!page.favicon_url.is_empty())
    dict->SetString(L"faviconUrl", page.favicon_url.spec());
  if (!page.thumbnail_url.is_empty())
    dict->SetString(L"thumbnailUrl", page.thumbnail_url.spec());
}

}  // namespace

MostVisitedHandler::MostVisitedHandler()
    : url_blacklist_(NULL),
      pinned_urls_(NULL),
      got_first_most_visited_request_(false) {
}

DOMMessageHandler* MostVisitedHandler::Attach(DOMUI* dom_ui) {
  url_blacklist_ = dom_ui->GetProfile()->GetPrefs()->
      GetMutableDictionary(prefs::kNTPMostVisitedURLsBlacklist);
  pinned_urls_ = dom_ui->GetProfile()->GetPrefs()->
      GetMutableDictionary(prefs::kNTPMostVisitedPinnedURLs);
  // Set up our sources for thumbnail and favicon data.
  DOMUIThumbnailSource* thumbnail_src =
      new DOMUIThumbnailSource(dom_ui->GetProfile());
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(Singleton<ChromeURLDataManager>::get(),
                        &ChromeURLDataManager::AddDataSource,
                        make_scoped_refptr(thumbnail_src)));

  DOMUIFavIconSource* favicon_src = new DOMUIFavIconSource(dom_ui->GetProfile());
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(Singleton<ChromeURLDataManager>::get(),
                        &ChromeURLDataManager::AddDataSource,
                        make_scoped_refptr(favicon_src)));

  // Get notifications when history is cleared.
  registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED,
      Source<Profile>(dom_ui->GetProfile()));

  DOMMessageHandler* result = DOMMessageHandler::Attach(dom_ui);

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

void MostVisitedHandler::HandleGetMostVisited(const Value* value) {
  if (!got_first_most_visited_request_) {
    // If our intial data is already here, return it.
    if (pages_value_.get()) {
      FundamentalValue first_run(IsFirstRun());
      dom_ui_->CallJavascriptFunction(L"mostVisitedPages",
                                      *(pages_value_.get()), first_run);
      pages_value_.reset();
    }
    got_first_most_visited_request_ = true;
  } else {
    StartQueryForMostVisited();
  }
}

void MostVisitedHandler::StartQueryForMostVisited() {
  const int page_count = kMostVisitedPages;
  // Let's query for the number of items we want plus the blacklist size as
  // we'll be filtering-out the returned list with the blacklist URLs.
  // We do not subtract the number of pinned URLs we have because the
  // HistoryService does not know about those.
  const int result_count = page_count + url_blacklist_->size();
  HistoryService* hs =
      dom_ui_->GetProfile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  // |hs| may be null during unit tests.
  if (hs) {
    hs->QuerySegmentUsageSince(
        &cancelable_consumer_,
        base::Time::Now() - base::TimeDelta::FromDays(kMostVisitedScope),
        result_count,
        NewCallback(this, &MostVisitedHandler::OnSegmentUsageAvailable));
  }
}

void MostVisitedHandler::HandleBlacklistURL(const Value* value) {
  if (!value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }
  std::string url;
  const ListValue* list = static_cast<const ListValue*>(value);
  if (list->GetSize() == 0 || !list->GetString(0, &url)) {
    NOTREACHED();
    return;
  }
  BlacklistURL(GURL(url));
}

void MostVisitedHandler::HandleRemoveURLsFromBlacklist(const Value* urls) {
  if (!urls->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }
  const ListValue* list = static_cast<const ListValue*>(urls);
  if (list->GetSize() == 0) {
    NOTREACHED();
    return;
  }

  for (ListValue::const_iterator iter = list->begin();
       iter != list->end(); ++iter) {
    std::wstring url;
    bool r = (*iter)->GetAsString(&url);
    if (!r) {
      NOTREACHED();
      return;
    }
    r = url_blacklist_->Remove(GetDictionaryKeyForURL(WideToUTF8(url)), NULL);
    DCHECK(r) << "Unknown URL removed from the NTP Most Visited blacklist.";
  }
}

void MostVisitedHandler::HandleClearBlacklist(const Value* value) {
  url_blacklist_->Clear();
}

void MostVisitedHandler::HandleAddPinnedURL(const Value* value) {
  if (!value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }

  const ListValue* list = static_cast<const ListValue*>(value);
  DCHECK(list->GetSize() == 5) << "Wrong number of params to addPinnedURL";
  MostVisitedPage mvp;
  std::string tmp_string;
  int index;

  bool r = list->GetString(0, &tmp_string);
  DCHECK(r) << "Missing URL in addPinnedURL from the NTP Most Visited.";
  mvp.url = GURL(tmp_string);

  r = list->GetString(1, &tmp_string);
  DCHECK(r) << "Missing title in addPinnedURL from the NTP Most Visited.";
  mvp.title = UTF8ToWide(tmp_string);

  r = list->GetString(2, &tmp_string);
  DCHECK(r) << "Failed to read the favicon URL in addPinnedURL from the NTP "
            << "Most Visited.";
  if (!tmp_string.empty())
    mvp.favicon_url = GURL(tmp_string);

  r = list->GetString(3, &tmp_string);
  DCHECK(r) << "Failed to read the thumbnail URL in addPinnedURL from the NTP "
            << "Most Visited.";
  if (!tmp_string.empty())
    mvp.thumbnail_url = GURL(tmp_string);

  r = list->GetString(4, &tmp_string);
  DCHECK(r) << "Missing index in addPinnedURL from the NTP Most Visited.";
  index = StringToInt(tmp_string);

  AddPinnedURL(mvp, index);
}

void MostVisitedHandler::AddPinnedURL(const MostVisitedPage& page, int index) {
  // Remove any pinned URL at the given index.
  MostVisitedPage old_page;
  if (GetPinnedURLAtIndex(index, &old_page)) {
    RemovePinnedURL(old_page.url);
  }

  DictionaryValue* new_value = new DictionaryValue();
  SetMostVisistedPage(new_value, page);

  new_value->SetInteger(L"index", index);
  pinned_urls_->Set(GetDictionaryKeyForURL(page.url.spec()), new_value);

  // TODO(arv): Notify observers?

  // Don't call HandleGetMostVisited. Let the client call this as needed.
}

void MostVisitedHandler::HandleRemovePinnedURL(const Value* value) {
  if (!value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }

  const ListValue* list = static_cast<const ListValue*>(value);
  std::string url;

  bool r = list->GetString(0, &url);
  DCHECK(r) << "Failed to read the URL to remove from the NTP Most Visited.";

  RemovePinnedURL(GURL(url));
}

void MostVisitedHandler::RemovePinnedURL(const GURL& url) {
  const std::wstring key = GetDictionaryKeyForURL(url.spec());
  if (pinned_urls_->HasKey(key))
    pinned_urls_->Remove(key, NULL);

  // TODO(arv): Notify observers?

  // Don't call HandleGetMostVisited. Let the client call this as needed.
}

const bool MostVisitedHandler::GetPinnedURLAtIndex(const int index,
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
        NOTREACHED();
        return false;
      }

      int dict_index;
      DictionaryValue* dict = static_cast<DictionaryValue*>(value);
      if (dict->GetInteger(L"index", &dict_index) && dict_index == index) {
        // The favicon and thumbnail URLs may be empty.
        std::string tmp_string;
        if (dict->GetString(L"faviconUrl", &tmp_string))
          page->favicon_url = GURL(tmp_string);
        if (dict->GetString(L"thumbnailUrl", &tmp_string))
          page->thumbnail_url = GURL(tmp_string);

        if (dict->GetString(L"url", &tmp_string))
          page->url = GURL(tmp_string);
        else
          return false;

        return dict->GetString(L"title", &page->title);
      }
    } else {
      NOTREACHED() << "DictionaryValue iterators are filthy liars.";
    }
  }

  return false;
}

void MostVisitedHandler::OnSegmentUsageAvailable(
    CancelableRequestProvider::Handle handle,
    std::vector<PageUsageData*>* data) {
  most_visited_urls_.clear();
  pages_value_.reset(new ListValue);
  std::set<GURL> seen_urls;

  size_t data_index = 0;
  size_t output_index = 0;
  size_t pre_populated_index = 0;
  const size_t pages_count = kMostVisitedPages;
  const std::vector<MostVisitedPage> pre_populated_pages =
      MostVisitedHandler::GetPrePopulatedPages();

  while (output_index < pages_count) {
    bool found = false;
    bool pinned = false;
    std::string pinned_url;
    std::string pinned_title;
    MostVisitedPage mvp;

    if (MostVisitedHandler::GetPinnedURLAtIndex(output_index, &mvp)) {
      pinned = true;
      found = true;
    }

    while (!found && data_index < data->size()) {
      const PageUsageData& page = *(*data)[data_index];
      data_index++;
      mvp.url = page.GetURL();

      // Don't include blacklisted or pinned URLs.
      std::wstring key = GetDictionaryKeyForURL(mvp.url.spec());
      if (pinned_urls_->HasKey(key) || url_blacklist_->HasKey(key))
        continue;

      mvp.title = UTF16ToWide(page.GetTitle());
      found = true;
    }

    while (!found && pre_populated_index < pre_populated_pages.size()) {
      mvp = pre_populated_pages[pre_populated_index++];
      std::wstring key = GetDictionaryKeyForURL(mvp.url.spec());
      if (pinned_urls_->HasKey(key) || url_blacklist_->HasKey(key) ||
          seen_urls.find(mvp.url) != seen_urls.end())
        continue;

      found = true;
    }

    if (found) {
      // Add fillers as needed.
      while (pages_value_->GetSize() < output_index) {
        DictionaryValue* filler_value = new DictionaryValue();
        filler_value->SetBoolean(L"filler", true);
        pages_value_->Append(filler_value);
      }

      DictionaryValue* page_value = new DictionaryValue();
      SetMostVisistedPage(page_value, mvp);
      page_value->SetBoolean(L"pinned", pinned);
      pages_value_->Append(page_value);
      most_visited_urls_.push_back(mvp.url);
      seen_urls.insert(mvp.url);
    }
    output_index++;
  }

  if (got_first_most_visited_request_) {
    FundamentalValue first_run(IsFirstRun());
    dom_ui_->CallJavascriptFunction(L"mostVisitedPages", *(pages_value_.get()),
                                    first_run);
    pages_value_.reset();
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
        l10n_util::GetString(IDS_NEW_TAB_CHROME_WELCOME_PAGE_TITLE),
        GURL(WideToUTF8(l10n_util::GetString(IDS_CHROME_WELCOME_URL))),
        GURL("chrome://theme/newtab_chrome_welcome_page_thumbnail"),
        GURL("chrome://theme/newtab_chrome_welcome_page_favicon")};
    pages.push_back(welcome_page);

    MostVisitedPage gallery_page = {
        l10n_util::GetString(IDS_NEW_TAB_THEMES_GALLERY_PAGE_TITLE),
        GURL(WideToUTF8(l10n_util::GetString(IDS_THEMES_GALLERY_URL))),
        GURL("chrome://theme/newtab_themes_gallery_thumbnail"),
        GURL("chrome://theme/newtab_themes_gallery_favicon")};
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
  RemovePinnedURL(url);

  std::wstring key = GetDictionaryKeyForURL(url.spec());
  if (url_blacklist_->HasKey(key))
    return;
  url_blacklist_->SetBoolean(key, true);
}

std::wstring MostVisitedHandler::GetDictionaryKeyForURL(
    const std::string& url) {
  return ASCIIToWide(MD5String(url));
}

// static
void MostVisitedHandler::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kNTPMostVisitedURLsBlacklist);
  prefs->RegisterDictionaryPref(prefs::kNTPMostVisitedPinnedURLs);
}
