// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_ui.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
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
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/web_history_service.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
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
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif

using content::UserMetricsAction;
using content::WebContents;

static const char kStringsJsFile[] = "strings.js";
static const char kHistoryJsFile[] = "history.js";

// The amount of time to wait for a response from the WebHistoryService.
static const int kWebHistoryTimeoutSeconds = 3;

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

// Format the URL and title for the given history query result.
void SetURLAndTitle(DictionaryValue* result,
                    const string16& title,
                    const GURL& gurl) {
  result->SetString("url", gurl.spec());

  bool using_url_as_the_title = false;
  string16 title_to_set(title);
  if (title.empty()) {
    using_url_as_the_title = true;
    title_to_set = UTF8ToUTF16(gurl.spec());
  }

  // Since the title can contain BiDi text, we need to mark the text as either
  // RTL or LTR, depending on the characters in the string. If we use the URL
  // as the title, we mark the title as LTR since URLs are always treated as
  // left to right strings.
  if (base::i18n::IsRTL()) {
    if (using_url_as_the_title)
      base::i18n::WrapStringWithLTRFormatting(&title_to_set);
    else
      base::i18n::AdjustStringForLocaleDirection(&title_to_set);
  }
  result->SetString("title", title_to_set);
}

ChromeWebUIDataSource* CreateHistoryUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIHistoryFrameHost);
  source->AddLocalizedString("loading", IDS_HISTORY_LOADING);
  source->AddLocalizedString("title", IDS_HISTORY_TITLE);
  source->AddLocalizedString("newest", IDS_HISTORY_NEWEST);
  source->AddLocalizedString("newer", IDS_HISTORY_NEWER);
  source->AddLocalizedString("older", IDS_HISTORY_OLDER);
  source->AddLocalizedString("searchresultsfor", IDS_HISTORY_SEARCHRESULTSFOR);
  source->AddLocalizedString("history", IDS_HISTORY_BROWSERESULTS);
  source->AddLocalizedString("cont", IDS_HISTORY_CONTINUED);
  source->AddLocalizedString("searchbutton", IDS_HISTORY_SEARCH_BUTTON);
  source->AddLocalizedString("noresults", IDS_HISTORY_NO_RESULTS);
  source->AddLocalizedString("noitems", IDS_HISTORY_NO_ITEMS);
  source->AddLocalizedString("edithistory", IDS_HISTORY_START_EDITING_HISTORY);
  source->AddLocalizedString("doneediting", IDS_HISTORY_STOP_EDITING_HISTORY);
  source->AddLocalizedString("removeselected",
                             IDS_HISTORY_REMOVE_SELECTED_ITEMS);
  source->AddLocalizedString("clearallhistory",
                             IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG);
  source->AddString(
      "deletewarning",
      l10n_util::GetStringFUTF16(IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING,
                                 UTF8ToUTF16(kIncognitoModeShortcut)));
  source->AddLocalizedString("actionMenuDescription",
                             IDS_HISTORY_ACTION_MENU_DESCRIPTION);
  source->AddLocalizedString("removeFromHistory", IDS_HISTORY_REMOVE_PAGE);
  source->AddLocalizedString("moreFromSite", IDS_HISTORY_MORE_FROM_SITE);
  source->set_json_path(kStringsJsFile);
  source->add_resource_path(kHistoryJsFile, IDR_HISTORY_JS);
  source->set_default_resource(IDR_HISTORY_HTML);
  source->set_use_json_js_format_v2();
  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// BrowsingHistoryHandler
//
////////////////////////////////////////////////////////////////////////////////
BrowsingHistoryHandler::BrowsingHistoryHandler() {}

BrowsingHistoryHandler::~BrowsingHistoryHandler() {
  history_request_consumer_.CancelAllRequests();
  web_history_request_.reset();
}

void BrowsingHistoryHandler::RegisterMessages() {
  // Create our favicon data source.
  Profile* profile = Profile::FromWebUI(web_ui());
  ChromeURLDataManager::AddDataSource(profile,
      new FaviconSource(profile, FaviconSource::FAVICON));

  // Get notifications when history is cleared.
  registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
      content::Source<Profile>(profile->GetOriginalProfile()));

  web_ui()->RegisterMessageCallback("queryHistory",
      base::Bind(&BrowsingHistoryHandler::HandleQueryHistory,
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

bool BrowsingHistoryHandler::ExtractIntegerValueAtIndex(const ListValue* value,
                                                        int index,
                                                        int* out_int) {
  double double_value;
  if (value->GetDouble(index, &double_value)) {
    *out_int = static_cast<int>(double_value);
    return true;
  }
  NOTREACHED();
  return false;
}

void BrowsingHistoryHandler::WebHistoryTimeout() {
  // TODO(dubroy): Communicate the failure to the front end.
  if (!results_info_value_.empty())
    ReturnResultsToFrontEnd();
}

void BrowsingHistoryHandler::QueryHistory(
    string16 search_text, const history::QueryOptions& options) {
  Profile* profile = Profile::FromWebUI(web_ui());

  // Anything in-flight is invalid.
  history_request_consumer_.CancelAllRequests();
  web_history_request_.reset();

  results_value_.Clear();
  results_info_value_.Clear();

  HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text,
      options,
      &history_request_consumer_,
      base::Bind(&BrowsingHistoryHandler::QueryComplete,
                 base::Unretained(this), search_text, options));

  history::WebHistoryService* web_history =
      WebHistoryServiceFactory::GetForProfile(profile);
  if (web_history) {
    web_history_request_ = web_history->QueryHistory(
        search_text,
        options,
        base::Bind(&BrowsingHistoryHandler::WebHistoryQueryComplete,
                   base::Unretained(this), search_text, options));
    // Start a timer so we know when to give up.
    web_history_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kWebHistoryTimeoutSeconds),
        this, &BrowsingHistoryHandler::WebHistoryTimeout);
  }
}

void BrowsingHistoryHandler::HandleQueryHistory(const ListValue* args) {
  history::QueryOptions options;

  // Parse the arguments from JavaScript. There are four required arguments:
  // - the text to search for (may be empty)
  // - the end time of the range to search (see QueryOptions.end_time)
  // - the search cursor, an opaque value from a previous query result, which
  //   allows this query to pick up where the previous one left off. May be
  //   null or undefined.
  // - the maximum number of results to return (may be 0, meaning that there
  //   is no maximum)
  string16 search_text = ExtractStringValue(args);

  double end_time;
  if (!args->GetDouble(1, &end_time))
    return;
  if (end_time)
    options.end_time = base::Time::FromJsTime(end_time);

  const Value* cursor_value;

  // Get the cursor. It must be either null, or a list.
  if (!args->Get(2, &cursor_value) ||
      (!cursor_value->IsType(Value::TYPE_NULL) &&
      !history::QueryCursor::FromValue(cursor_value, &options.cursor))) {
    NOTREACHED() << "Failed to convert argument 2. ";
    return;
  }

  if (!ExtractIntegerValueAtIndex(args, 3, &options.max_count)) {
    NOTREACHED() << "Failed to convert argument 3.";
    return;
  }

  options.duplicate_policy = history::QueryOptions::REMOVE_DUPLICATES_PER_DAY;
  QueryHistory(search_text, options);
}

void BrowsingHistoryHandler::HandleRemoveURLsOnOneDay(const ListValue* args) {
  if (delete_task_tracker_.HasTrackedTasks()) {
    web_ui()->CallJavascriptFunction("deleteFailed");
    return;
  }

  // Get day to delete data from.
  double visit_time = 0;
  if (!ExtractDoubleValue(args, &visit_time)) {
    LOG(ERROR) << "Unable to extract double argument.";
    web_ui()->CallJavascriptFunction("deleteFailed");
    return;
  }
  base::Time::Exploded exploded;
  base::Time::FromJsTime(visit_time).LocalExplode(&exploded);
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

  HistoryService* hs = HistoryServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()), Profile::EXPLICIT_ACCESS);
  hs->ExpireHistoryBetween(
      urls_to_be_deleted_, begin_time, end_time,
      base::Bind(&BrowsingHistoryHandler::RemoveComplete,
                 base::Unretained(this)),
      &delete_task_tracker_);
}

void BrowsingHistoryHandler::HandleClearBrowsingData(const ListValue* args) {
#if defined(OS_ANDROID)
  Profile* profile = Profile::FromWebUI(web_ui());
  const TabModel* tab_model =
      TabModelList::GetTabModelWithProfile(profile);
  if (tab_model)
    tab_model->OpenClearBrowsingData();
#else
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  chrome::ShowClearBrowsingDataDialog(browser);
#endif
}

void BrowsingHistoryHandler::HandleRemoveBookmark(const ListValue* args) {
  string16 url = ExtractStringValue(args);
  Profile* profile = Profile::FromWebUI(web_ui());
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile);
  bookmark_utils::RemoveAllBookmarks(model, GURL(url));
}

DictionaryValue* BrowsingHistoryHandler::CreateQueryResultValue(
    const GURL& url, const string16 title, base::Time visit_time,
    bool is_search_result, const string16& snippet) {
  DictionaryValue* result = new DictionaryValue();
  SetURLAndTitle(result, title, url);
  result->SetDouble("time", visit_time.ToJsTime());

  // Until we get some JS i18n infrastructure, we also need to
  // pass the dates in as strings. This could use some
  // optimization.

  // Only pass in the strings we need (search results need a shortdate
  // and snippet, browse results need day and time information).
  if (is_search_result) {
    result->SetString("dateShort", base::TimeFormatShortDate(visit_time));
    result->SetString("snippet", snippet);
  } else {
    base::Time midnight_today = base::Time::Now().LocalMidnight();
    string16 date_str = TimeFormat::RelativeDate(visit_time, &midnight_today);
    if (date_str.empty()) {
      date_str = base::TimeFormatFriendlyDate(visit_time);
    } else {
      date_str = l10n_util::GetStringFUTF16(
          IDS_HISTORY_DATE_WITH_RELATIVE_TIME,
          date_str,
          base::TimeFormatFriendlyDate(visit_time));
    }
    result->SetString("dateRelativeDay", date_str);
    result->SetString("dateTimeOfDay", base::TimeFormatTimeOfDay(visit_time));
  }
  Profile* profile = Profile::FromWebUI(web_ui());
  result->SetBoolean("starred",
      BookmarkModelFactory::GetForProfile(profile)->IsBookmarked(url));

  return result;
}

void BrowsingHistoryHandler::ReturnResultsToFrontEnd() {
  web_ui()->CallJavascriptFunction(
      "historyResult", results_info_value_, results_value_);
  results_info_value_.Clear();
  results_value_.Clear();
}

void BrowsingHistoryHandler::QueryComplete(
    const string16& search_text,
    const history::QueryOptions& options,
    HistoryService::Handle request_handle,
    history::QueryResults* results) {
  unsigned int old_results_count = results_value_.GetSize();

  for (size_t i = 0; i < results->size(); ++i) {
    history::URLResult const &page = (*results)[i];
    results_value_.Append(
        CreateQueryResultValue(
            page.url(), page.title(), page.visit_time(), !search_text.empty(),
            page.snippet().text()));
  }

  results_info_value_.SetString("term", search_text);
  results_info_value_.SetBoolean("finished", results->reached_beginning());
  results_info_value_.Set("cursor", results->cursor().ToValue());

  // The results are sorted if and only if they were empty before.
  results_info_value_.SetBoolean("sorted", old_results_count == 0);

  if (!web_history_timer_.IsRunning())
    ReturnResultsToFrontEnd();
}

void BrowsingHistoryHandler::WebHistoryQueryComplete(
    const string16& search_text,
    const history::QueryOptions& options,
    history::WebHistoryService::Request* request,
    const DictionaryValue* results_value) {
  web_history_timer_.Stop();

  // Check if the results have already been received from the history DB.
  // It is unlikely, but possible, that server results will arrive first.
  bool has_local_results = !results_info_value_.empty();
  unsigned int old_results_count = results_value_.GetSize();

  const ListValue* events = NULL;
  if (results_value && results_value->GetList("event", &events)) {
    for (unsigned int i = 0; i < events->GetSize(); ++i) {
      const DictionaryValue* event = NULL;
      const DictionaryValue* result = NULL;
      const DictionaryValue* id = NULL;
      const ListValue* results = NULL;
      const ListValue* ids = NULL;
      string16 timestamp_string;
      string16 url;
      int64 timestamp_usec;

      if (!events->GetDictionary(i, &event) ||
          !event->GetList("result", &results)) {
        LOG(WARNING) << "Improperly formed JSON response.";
        continue;  // Skip this result.
      }

      DCHECK_GT(results->GetSize(), 0U);

      if (results->GetDictionary(0, &result) &&
          result->GetList("id", &ids) &&
          result->GetStringWithoutPathExpansion("url", &url) &&
          ids->GetDictionary(0, &id) &&
          id->GetString("timestamp_usec", &timestamp_string) &&
          base::StringToInt64(timestamp_string, &timestamp_usec)) {
        results_value_.Append(
            CreateQueryResultValue(
                GURL(url),
                string16(),
                base::Time::FromJsTime(timestamp_usec / 1000),
                !search_text.empty(),
                string16()));
      }
    }
    // The results are sorted if and only if they were empty before.
    results_info_value_.SetBoolean("sorted", old_results_count == 0);
  } else if (results_value) {
    NOTREACHED() << "Failed to parse JSON response.";
  }

  if (has_local_results)
    ReturnResultsToFrontEnd();
}

void BrowsingHistoryHandler::RemoveComplete() {
  urls_to_be_deleted_.clear();

  // Notify the page that the deletion request succeeded.
  web_ui()->CallJavascriptFunction("deleteComplete");
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
  ChromeURLDataManager::AddDataSource(Profile::FromWebUI(web_ui),
                                      CreateHistoryUIHTMLSource());
}

// static
const GURL HistoryUI::GetHistoryURLWithSearchText(const string16& text) {
  return GURL(std::string(chrome::kChromeUIHistoryURL) + "#q=" +
              net::EscapeQueryParamValue(UTF16ToUTF8(text), true));
}

// static
base::RefCountedMemory* HistoryUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_HISTORY_FAVICON, scale_factor);
}
