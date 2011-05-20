// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_ui.h"

#include <algorithm>
#include <set>

#include "base/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/user_metrics.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// Maximum number of search results to return in a given search. We should
// eventually remove this.
static const int kMaxSearchResults = 100;
static const char kStringsJsFile[] = "strings.js";
static const char kHistoryJsFile[] = "history.js";

////////////////////////////////////////////////////////////////////////////////
//
// HistoryUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////
class HistoryUIHTMLSource : public ChromeWebUIDataSource {
 public:
  HistoryUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);

  virtual std::string GetMimeType(const std::string& path) const;

 private:
  ~HistoryUIHTMLSource();

  DISALLOW_COPY_AND_ASSIGN(HistoryUIHTMLSource);
};


HistoryUIHTMLSource::HistoryUIHTMLSource()
    : ChromeWebUIDataSource(chrome::kChromeUIHistoryHost) {
  AddLocalizedString("loading", IDS_HISTORY_LOADING);
  AddLocalizedString("title", IDS_HISTORY_TITLE);
  AddLocalizedString("loading", IDS_HISTORY_LOADING);
  AddLocalizedString("newest", IDS_HISTORY_NEWEST);
  AddLocalizedString("newer",  IDS_HISTORY_NEWER);
  AddLocalizedString("older", IDS_HISTORY_OLDER);
  AddLocalizedString("searchresultsfor", IDS_HISTORY_SEARCHRESULTSFOR);
  AddLocalizedString("history", IDS_HISTORY_BROWSERESULTS);
  AddLocalizedString("cont", IDS_HISTORY_CONTINUED);
  AddLocalizedString("searchbutton", IDS_HISTORY_SEARCH_BUTTON);
  AddLocalizedString("noresults", IDS_HISTORY_NO_RESULTS);
  AddLocalizedString("noitems", IDS_HISTORY_NO_ITEMS);
  AddLocalizedString("edithistory", IDS_HISTORY_START_EDITING_HISTORY);
  AddLocalizedString("doneediting", IDS_HISTORY_STOP_EDITING_HISTORY);
  AddLocalizedString("removeselected", IDS_HISTORY_REMOVE_SELECTED_ITEMS);
  AddLocalizedString("clearallhistory",
                     IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG);
  AddLocalizedString("deletewarning",
                     IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING);
}

HistoryUIHTMLSource::~HistoryUIHTMLSource() {
}

void HistoryUIHTMLSource::StartDataRequest(const std::string& path,
                                           bool is_incognito,
                                           int request_id) {
  if (path == kStringsJsFile) {
    SendLocalizedStringsAsJSON(request_id);
  } else {
    int idr = path == kHistoryJsFile ? IDR_HISTORY_JS : IDR_HISTORY_HTML;
    SendFromResourceBundle(request_id, idr);
  }
}

std::string HistoryUIHTMLSource::GetMimeType(const std::string& path) const {
  if (path == kStringsJsFile || path == kHistoryJsFile)
    return "application/javascript";

  return "text/html";
}

////////////////////////////////////////////////////////////////////////////////
//
// BrowsingHistoryHandler
//
////////////////////////////////////////////////////////////////////////////////
BrowsingHistoryHandler::BrowsingHistoryHandler()
    : search_text_() {
}

BrowsingHistoryHandler::~BrowsingHistoryHandler() {
  cancelable_search_consumer_.CancelAllRequests();
  cancelable_delete_consumer_.CancelAllRequests();
}

WebUIMessageHandler* BrowsingHistoryHandler::Attach(WebUI* web_ui) {
  // Create our favicon data source.
  Profile* profile = web_ui->GetProfile();
  profile->GetChromeURLDataManager()->AddDataSource(
      new FaviconSource(profile, FaviconSource::FAVICON));

  return WebUIMessageHandler::Attach(web_ui);
}

void BrowsingHistoryHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getHistory",
      NewCallback(this, &BrowsingHistoryHandler::HandleGetHistory));
  web_ui_->RegisterMessageCallback("searchHistory",
      NewCallback(this, &BrowsingHistoryHandler::HandleSearchHistory));
  web_ui_->RegisterMessageCallback("removeURLsOnOneDay",
      NewCallback(this, &BrowsingHistoryHandler::HandleRemoveURLsOnOneDay));
  web_ui_->RegisterMessageCallback("clearBrowsingData",
      NewCallback(this, &BrowsingHistoryHandler::HandleClearBrowsingData));
}

void BrowsingHistoryHandler::HandleGetHistory(const ListValue* args) {
  // Anything in-flight is invalid.
  cancelable_search_consumer_.CancelAllRequests();

  // Get arguments (if any).
  int day = 0;
  ExtractIntegerValue(args, &day);

  // Set our query options.
  history::QueryOptions options;
  options.begin_time = base::Time::Now().LocalMidnight();
  options.begin_time -= base::TimeDelta::FromDays(day);
  options.end_time = base::Time::Now().LocalMidnight();
  options.end_time -= base::TimeDelta::FromDays(day - 1);

  // Need to remember the query string for our results.
  search_text_ = string16();

  HistoryService* hs =
      web_ui_->GetProfile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text_,
      options,
      &cancelable_search_consumer_,
      NewCallback(this, &BrowsingHistoryHandler::QueryComplete));
}

void BrowsingHistoryHandler::HandleSearchHistory(const ListValue* args) {
  // Anything in-flight is invalid.
  cancelable_search_consumer_.CancelAllRequests();

  // Get arguments (if any).
  int month = 0;
  string16 query;
  ExtractSearchHistoryArguments(args, &month, &query);

  // Set the query ranges for the given month.
  history::QueryOptions options = CreateMonthQueryOptions(month);

  // When searching, limit the number of results returned.
  options.max_count = kMaxSearchResults;

  // Need to remember the query string for our results.
  search_text_ = query;
  HistoryService* hs =
      web_ui_->GetProfile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text_,
      options,
      &cancelable_search_consumer_,
      NewCallback(this, &BrowsingHistoryHandler::QueryComplete));
}

void BrowsingHistoryHandler::HandleRemoveURLsOnOneDay(const ListValue* args) {
  if (cancelable_delete_consumer_.HasPendingRequests()) {
    web_ui_->CallJavascriptFunction("deleteFailed");
    return;
  }

  // Get day to delete data from.
  int visit_time = 0;
  ExtractIntegerValue(args, &visit_time);
  base::Time::Exploded exploded;
  base::Time::FromTimeT(
      static_cast<time_t>(visit_time)).LocalExplode(&exploded);
  exploded.hour = exploded.minute = exploded.second = exploded.millisecond = 0;
  base::Time begin_time = base::Time::FromLocalExploded(exploded);
  base::Time end_time = begin_time + base::TimeDelta::FromDays(1);

  // Get URLs.
  std::set<GURL> urls;
  for (ListValue::const_iterator v = args->begin() + 1;
       v != args->end(); ++v) {
    if ((*v)->GetType() != Value::TYPE_STRING)
      continue;
    const StringValue* string_value = static_cast<const StringValue*>(*v);
    string16 string16_value;
    if (!string_value->GetAsString(&string16_value))
      continue;
    urls.insert(GURL(string16_value));
  }

  HistoryService* hs =
      web_ui_->GetProfile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->ExpireHistoryBetween(
      urls, begin_time, end_time, &cancelable_delete_consumer_,
      NewCallback(this, &BrowsingHistoryHandler::RemoveComplete));
}

void BrowsingHistoryHandler::HandleClearBrowsingData(const ListValue* args) {
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Browser* browser = BrowserList::FindBrowserWithProfile(web_ui_->GetProfile());
  if (browser)
    browser->OpenClearBrowsingDataDialog();
}

void BrowsingHistoryHandler::QueryComplete(
    HistoryService::Handle request_handle,
    history::QueryResults* results) {

  ListValue results_value;
  base::Time midnight_today = base::Time::Now().LocalMidnight();

  for (size_t i = 0; i < results->size(); ++i) {
    history::URLResult const &page = (*results)[i];
    DictionaryValue* page_value = new DictionaryValue();
    SetURLAndTitle(page_value, page.title(), page.url());

    // Need to pass the time in epoch time (fastest JS conversion).
    page_value->SetInteger("time",
        static_cast<int>(page.visit_time().ToTimeT()));

    // Until we get some JS i18n infrastructure, we also need to
    // pass the dates in as strings. This could use some
    // optimization.

    // Only pass in the strings we need (search results need a shortdate
    // and snippet, browse results need day and time information).
    if (search_text_.empty()) {
      // Figure out the relative date string.
      string16 date_str = TimeFormat::RelativeDate(page.visit_time(),
                                                   &midnight_today);
      if (date_str.empty()) {
        date_str = base::TimeFormatFriendlyDate(page.visit_time());
      } else {
        date_str = l10n_util::GetStringFUTF16(
            IDS_HISTORY_DATE_WITH_RELATIVE_TIME,
            date_str,
            base::TimeFormatFriendlyDate(page.visit_time()));
      }
      page_value->SetString("dateRelativeDay", date_str);
      page_value->SetString("dateTimeOfDay",
          base::TimeFormatTimeOfDay(page.visit_time()));
    } else {
      page_value->SetString("dateShort",
          base::TimeFormatShortDate(page.visit_time()));
      page_value->SetString("snippet", page.snippet().text());
    }
    page_value->SetBoolean("starred",
        web_ui_->GetProfile()->GetBookmarkModel()->IsBookmarked(page.url()));
    results_value.Append(page_value);
  }

  DictionaryValue info_value;
  info_value.SetString("term", search_text_);
  info_value.SetBoolean("finished", results->reached_beginning());

  web_ui_->CallJavascriptFunction("historyResult", info_value, results_value);
}

void BrowsingHistoryHandler::RemoveComplete() {
  // Some Visits were deleted from history. Reload the list.
  web_ui_->CallJavascriptFunction("deleteComplete");
}

void BrowsingHistoryHandler::ExtractSearchHistoryArguments(
      const ListValue* args,
      int* month,
      string16* query) {
  CHECK(args->GetSize() == 2);
  query->clear();
  CHECK(args->GetString(0, query));

  string16 string16_value;
  CHECK(args->GetString(1, &string16_value));
  *month = 0;
  base::StringToInt(string16_value, month);
}

history::QueryOptions BrowsingHistoryHandler::CreateMonthQueryOptions(
    int month) {
  history::QueryOptions options;

  // Configure the begin point of the search to the start of the
  // current month.
  base::Time::Exploded exploded;
  base::Time::Now().LocalMidnight().LocalExplode(&exploded);
  exploded.day_of_month = 1;

  if (month == 0) {
    options.begin_time = base::Time::FromLocalExploded(exploded);

    // Set the end time of this first search to null (which will
    // show results from the future, should the user's clock have
    // been set incorrectly).
    options.end_time = base::Time();
  } else {
    // Set the end-time of this search to the end of the month that is
    // |depth| months before the search end point. The end time is not
    // inclusive, so we should feel free to set it to midnight on the
    // first day of the following month.
    exploded.month -= month - 1;
    while (exploded.month < 1) {
      exploded.month += 12;
      exploded.year--;
    }
    options.end_time = base::Time::FromLocalExploded(exploded);

    // Set the begin-time of the search to the start of the month
    // that is |depth| months prior to search_start_.
    if (exploded.month > 1) {
      exploded.month--;
    } else {
      exploded.month = 12;
      exploded.year--;
    }
    options.begin_time = base::Time::FromLocalExploded(exploded);
  }

  return options;
}

////////////////////////////////////////////////////////////////////////////////
//
// HistoryUI
//
////////////////////////////////////////////////////////////////////////////////

HistoryUI::HistoryUI(TabContents* contents) : WebUI(contents) {
  AddMessageHandler((new BrowsingHistoryHandler())->Attach(this));

  HistoryUIHTMLSource* html_source = new HistoryUIHTMLSource();

  // Set up the chrome://history/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

// static
const GURL HistoryUI::GetHistoryURLWithSearchText(const string16& text) {
  return GURL(std::string(chrome::kChromeUIHistoryURL) + "#q=" +
              EscapeQueryParamValue(UTF16ToUTF8(text), true));
}

// static
RefCountedMemory* HistoryUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_HISTORY_FAVICON);
}
