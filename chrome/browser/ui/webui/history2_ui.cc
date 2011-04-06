// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history2_ui.h"

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
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
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

////////////////////////////////////////////////////////////////////////////////
//
// HistoryHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

HistoryUIHTMLSource2::HistoryUIHTMLSource2()
    : DataSource(chrome::kChromeUIHistory2Host, MessageLoop::current()) {
}

void HistoryUIHTMLSource2::StartDataRequest(const std::string& path,
                                            bool is_incognito,
                                            int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString("loading",
      l10n_util::GetStringUTF16(IDS_HISTORY_LOADING));
  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_HISTORY_TITLE));
  localized_strings.SetString("loading",
      l10n_util::GetStringUTF16(IDS_HISTORY_LOADING));
  localized_strings.SetString("newest",
      l10n_util::GetStringUTF16(IDS_HISTORY_NEWEST));
  localized_strings.SetString("newer",
      l10n_util::GetStringUTF16(IDS_HISTORY_NEWER));
  localized_strings.SetString("older",
      l10n_util::GetStringUTF16(IDS_HISTORY_OLDER));
  localized_strings.SetString("searchresultsfor",
      l10n_util::GetStringUTF16(IDS_HISTORY_SEARCHRESULTSFOR));
  localized_strings.SetString("history",
      l10n_util::GetStringUTF16(IDS_HISTORY_BROWSERESULTS));
  localized_strings.SetString("cont",
      l10n_util::GetStringUTF16(IDS_HISTORY_CONTINUED));
  localized_strings.SetString("searchbutton",
      l10n_util::GetStringUTF16(IDS_HISTORY_SEARCH_BUTTON));
  localized_strings.SetString("noresults",
      l10n_util::GetStringUTF16(IDS_HISTORY_NO_RESULTS));
  localized_strings.SetString("noitems",
      l10n_util::GetStringUTF16(IDS_HISTORY_NO_ITEMS));
  localized_strings.SetString("edithistory",
      l10n_util::GetStringUTF16(IDS_HISTORY_START_EDITING_HISTORY));
  localized_strings.SetString("doneediting",
      l10n_util::GetStringUTF16(IDS_HISTORY_STOP_EDITING_HISTORY));
  localized_strings.SetString("removeselected",
      l10n_util::GetStringUTF16(IDS_HISTORY_REMOVE_SELECTED_ITEMS));
  localized_strings.SetString("clearallhistory",
      l10n_util::GetStringUTF16(IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG));
  localized_strings.SetString("deletewarning",
      l10n_util::GetStringUTF16(IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING));

  SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece history_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_HISTORY2_HTML));
  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      history_html, &localized_strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

std::string HistoryUIHTMLSource2::GetMimeType(const std::string&) const {
  return "text/html";
}

////////////////////////////////////////////////////////////////////////////////
//
// HistoryHandler
//
////////////////////////////////////////////////////////////////////////////////
BrowsingHistoryHandler2::BrowsingHistoryHandler2()
    : search_text_() {
}

BrowsingHistoryHandler2::~BrowsingHistoryHandler2() {
  cancelable_search_consumer_.CancelAllRequests();
  cancelable_delete_consumer_.CancelAllRequests();
}

WebUIMessageHandler* BrowsingHistoryHandler2::Attach(WebUI* web_ui) {
  // Create our favicon data source.
  Profile* profile = web_ui->GetProfile();
  profile->GetChromeURLDataManager()->AddDataSource(
      new FaviconSource(profile));

  return WebUIMessageHandler::Attach(web_ui);
}

void BrowsingHistoryHandler2::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getHistory",
      NewCallback(this, &BrowsingHistoryHandler2::HandleGetHistory));
  web_ui_->RegisterMessageCallback("searchHistory",
      NewCallback(this, &BrowsingHistoryHandler2::HandleSearchHistory));
  web_ui_->RegisterMessageCallback("removeURLsOnOneDay",
      NewCallback(this, &BrowsingHistoryHandler2::HandleRemoveURLsOnOneDay));
  web_ui_->RegisterMessageCallback("clearBrowsingData",
      NewCallback(this, &BrowsingHistoryHandler2::HandleClearBrowsingData));
}

void BrowsingHistoryHandler2::HandleGetHistory(const ListValue* args) {
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
      NewCallback(this, &BrowsingHistoryHandler2::QueryComplete));
}

void BrowsingHistoryHandler2::HandleSearchHistory(const ListValue* args) {
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
      NewCallback(this, &BrowsingHistoryHandler2::QueryComplete));
}

void BrowsingHistoryHandler2::HandleRemoveURLsOnOneDay(const ListValue* args) {
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
      NewCallback(this, &BrowsingHistoryHandler2::RemoveComplete));
}

void BrowsingHistoryHandler2::HandleClearBrowsingData(const ListValue* args) {
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Browser* browser = BrowserList::FindBrowserWithProfile(web_ui_->GetProfile());
  if (browser)
    browser->OpenClearBrowsingDataDialog();
}

void BrowsingHistoryHandler2::QueryComplete(
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

void BrowsingHistoryHandler2::RemoveComplete() {
  // Some Visits were deleted from history. Reload the list.
  web_ui_->CallJavascriptFunction("deleteComplete");
}

void BrowsingHistoryHandler2::ExtractSearchHistoryArguments(
    const ListValue* args,
    int* month,
    string16* query) {
  *month = 0;
  Value* list_member;

  // Get search string.
  if (args->Get(0, &list_member) &&
      list_member->GetType() == Value::TYPE_STRING) {
    const StringValue* string_value =
      static_cast<const StringValue*>(list_member);
    string_value->GetAsString(query);
  }

  // Get search month.
  if (args->Get(1, &list_member) &&
      list_member->GetType() == Value::TYPE_STRING) {
    const StringValue* string_value =
      static_cast<const StringValue*>(list_member);
    string16 string16_value;
    string_value->GetAsString(&string16_value);
    base::StringToInt(string16_value, month);
  }
}

history::QueryOptions BrowsingHistoryHandler2::CreateMonthQueryOptions(
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
// HistoryUIContents
//
////////////////////////////////////////////////////////////////////////////////

HistoryUI2::HistoryUI2(TabContents* contents) : WebUI(contents) {
  AddMessageHandler((new BrowsingHistoryHandler2())->Attach(this));

  HistoryUIHTMLSource2* html_source = new HistoryUIHTMLSource2();

  // Set up the chrome://history2/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

// static
const GURL HistoryUI2::GetHistoryURLWithSearchText(const string16& text) {
  return GURL(std::string(chrome::kChromeUIHistory2URL) + "#q=" +
              EscapeQueryParamValue(UTF16ToUTF8(text), true));
}

// static
RefCountedMemory* HistoryUI2::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_HISTORY_FAVICON);
}
