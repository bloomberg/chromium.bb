// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_ui.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
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
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::UserMetricsAction;
using content::WebContents;

// Maximum number of search results to return in a given search. We should
// eventually remove this.
static const int kMaxSearchResults = 100;
static const char kStringsJsFile[] = "strings.js";
static const char kHistoryJsFile[] = "history.js";

namespace {

#if defined(OS_MACOSX)
const char kIncognitoModeShortcut[] = "("
    "\xE2\x87\xA7"  // Shift symbol (U+21E7 'UPWARDS WHITE ARROW').
    "\xE2\x8C\x98"  // Command symbol (U+2318 'PLACE OF INTEREST SIGN').
    "N)";
#elif defined(OS_WIN)
const char kIncognitoModeShortcut[] = "(Ctrl+Shift+N)";
#else
const char kIncognitoModeShortcut[] = "(Shift+Ctrl+N)";
#endif

};  // namespace

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
                                int request_id) OVERRIDE;

  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;

 private:
  ~HistoryUIHTMLSource();

  DISALLOW_COPY_AND_ASSIGN(HistoryUIHTMLSource);
};

HistoryUIHTMLSource::HistoryUIHTMLSource()
    : ChromeWebUIDataSource(chrome::kChromeUIHistoryFrameHost) {
  AddLocalizedString("loading", IDS_HISTORY_LOADING);
  AddLocalizedString("title", IDS_HISTORY_TITLE);
  AddLocalizedString("newest", IDS_HISTORY_NEWEST);
  AddLocalizedString("newer", IDS_HISTORY_NEWER);
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
  AddString("deletewarning",
      l10n_util::GetStringFUTF16(IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING,
                                 UTF8ToUTF16(kIncognitoModeShortcut)));
  AddLocalizedString("actionMenuDescription",
                     IDS_HISTORY_ACTION_MENU_DESCRIPTION);
  AddLocalizedString("removeFromHistory", IDS_HISTORY_REMOVE_PAGE);
  AddLocalizedString("moreFromSite", IDS_HISTORY_MORE_FROM_SITE);
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

void BrowsingHistoryHandler::RegisterMessages() {
  // Create our favicon data source.
  Profile* profile = Profile::FromWebUI(web_ui());
  ChromeURLDataManager::AddDataSource(profile,
      new FaviconSource(profile, FaviconSource::FAVICON));

  // Get notifications when history is cleared.
  registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
      content::Source<Profile>(profile->GetOriginalProfile()));

  web_ui()->RegisterMessageCallback("getHistory",
      base::Bind(&BrowsingHistoryHandler::HandleGetHistory,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("searchHistory",
      base::Bind(&BrowsingHistoryHandler::HandleSearchHistory,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeURLsOnOneDay",
      base::Bind(&BrowsingHistoryHandler::HandleRemoveURLsOnOneDay,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("clearBrowsingData",
      base::Bind(&BrowsingHistoryHandler::HandleClearBrowsingData,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeBookmark",
      base::Bind(&BrowsingHistoryHandler::HandleRemoveBookmark,
                 base::Unretained(this)));
}

void BrowsingHistoryHandler::HandleGetHistory(const ListValue* args) {
  // Anything in-flight is invalid.
  cancelable_search_consumer_.CancelAllRequests();

  // Get arguments (if any).
  int day = 0;
  bool res = ExtractIntegerValue(args, &day);
  DCHECK(res);

  // Set our query options.
  history::QueryOptions options;
  options.begin_time = base::Time::Now().LocalMidnight();
  options.begin_time -= base::TimeDelta::FromDays(day);
  options.end_time = base::Time::Now().LocalMidnight();
  options.end_time -= base::TimeDelta::FromDays(day - 1);

  // Need to remember the query string for our results.
  search_text_ = string16();

  HistoryService* hs =
      Profile::FromWebUI(web_ui())->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text_,
      options,
      &cancelable_search_consumer_,
      base::Bind(&BrowsingHistoryHandler::QueryComplete,
                 base::Unretained(this)));
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
      Profile::FromWebUI(web_ui())->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text_,
      options,
      &cancelable_search_consumer_,
      base::Bind(&BrowsingHistoryHandler::QueryComplete,
                 base::Unretained(this)));
}

void BrowsingHistoryHandler::HandleRemoveURLsOnOneDay(const ListValue* args) {
  if (cancelable_delete_consumer_.HasPendingRequests()) {
    web_ui()->CallJavascriptFunction("deleteFailed");
    return;
  }

  // Get day to delete data from.
  int visit_time = 0;
  if (!ExtractIntegerValue(args, &visit_time)) {
    LOG(ERROR) << "Unable to extract integer argument.";
    web_ui()->CallJavascriptFunction("deleteFailed");
    return;
  }
  base::Time::Exploded exploded;
  base::Time::FromTimeT(
      static_cast<time_t>(visit_time)).LocalExplode(&exploded);
  exploded.hour = exploded.minute = exploded.second = exploded.millisecond = 0;
  base::Time begin_time = base::Time::FromLocalExploded(exploded);
  base::Time end_time = begin_time + base::TimeDelta::FromDays(1);

  // Get URLs.
  DCHECK(urls_to_be_deleted_.empty());
  for (ListValue::const_iterator v = args->begin() + 1;
       v != args->end(); ++v) {
    if ((*v)->GetType() != Value::TYPE_STRING)
      continue;
    const StringValue* string_value = static_cast<const StringValue*>(*v);
    string16 string16_value;
    if (!string_value->GetAsString(&string16_value))
      continue;

    urls_to_be_deleted_.insert(GURL(string16_value));
  }

  HistoryService* hs =
      Profile::FromWebUI(web_ui())->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->ExpireHistoryBetween(
      urls_to_be_deleted_, begin_time, end_time, &cancelable_delete_consumer_,
      base::Bind(&BrowsingHistoryHandler::RemoveComplete,
                 base::Unretained(this)));
}

void BrowsingHistoryHandler::HandleClearBrowsingData(const ListValue* args) {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED() << "TODO(yfriedman): Upstream the Android version.";
#else
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Profile* profile = Profile::FromWebUI(web_ui());
  Browser* browser = browser::FindBrowserWithProfile(profile);
  if (browser)
    browser->OpenClearBrowsingDataDialog();
#endif
}

void BrowsingHistoryHandler::HandleRemoveBookmark(const ListValue* args) {
  string16 url = ExtractStringValue(args);
  Profile* profile = Profile::FromWebUI(web_ui());
  BookmarkModel* model = profile->GetBookmarkModel();
  bookmark_utils::RemoveAllBookmarks(model, GURL(url));
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
    Profile* profile = Profile::FromWebUI(web_ui());
    page_value->SetBoolean("starred",
        profile->GetBookmarkModel()->IsBookmarked(page.url()));
    results_value.Append(page_value);
  }

  DictionaryValue info_value;
  info_value.SetString("term", search_text_);
  info_value.SetBoolean("finished", results->reached_beginning());

  web_ui()->CallJavascriptFunction("historyResult", info_value, results_value);
}

void BrowsingHistoryHandler::RemoveComplete() {
  urls_to_be_deleted_.clear();

  // Notify the page that the deletion request succeeded.
  web_ui()->CallJavascriptFunction("deleteComplete");
}

void BrowsingHistoryHandler::ExtractSearchHistoryArguments(
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
    if (string_value->GetAsString(&string16_value)) {
      bool converted = base::StringToInt(string16_value, month);
      DCHECK(converted);
    }
  }
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

// Helper function for Observe that determines if there are any differences
// between the URLs noticed for deletion and the ones we are expecting.
static bool DeletionsDiffer(const history::URLRows& deleted_rows,
                            const std::set<GURL>& urls_to_be_deleted) {
  if (deleted_rows.size() != urls_to_be_deleted.size())
    return true;
  for (history::URLRows::const_iterator i = deleted_rows.begin();
       i != deleted_rows.end(); ++i) {
    if (urls_to_be_deleted.find(i->url()) == urls_to_be_deleted.end())
      return true;
  }
  return false;
}

void BrowsingHistoryHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_HISTORY_URLS_DELETED) {
    NOTREACHED();
    return;
  }
  history::URLsDeletedDetails* deletedDetails =
      content::Details<history::URLsDeletedDetails>(details).ptr();
  if (deletedDetails->all_history ||
      DeletionsDiffer(deletedDetails->rows, urls_to_be_deleted_))
    web_ui()->CallJavascriptFunction("historyDeleted");
}

////////////////////////////////////////////////////////////////////////////////
//
// HistoryUI
//
////////////////////////////////////////////////////////////////////////////////

HistoryUI::HistoryUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new BrowsingHistoryHandler());

  // Set up the chrome://history-frame/ source.
  HistoryUIHTMLSource* html_source = new HistoryUIHTMLSource();
  html_source->set_use_json_js_format_v2();
  ChromeURLDataManager::AddDataSource(Profile::FromWebUI(web_ui), html_source);
}

// static
const GURL HistoryUI::GetHistoryURLWithSearchText(const string16& text) {
  return GURL(std::string(chrome::kChromeUIHistoryURL) + "#q=" +
              net::EscapeQueryParamValue(UTF16ToUTF8(text), true));
}

// static
base::RefCountedMemory* HistoryUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_HISTORY_FAVICON);
}
