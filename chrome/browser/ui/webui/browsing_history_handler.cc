// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browsing_history_handler.h"

#include <stddef.h>

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/query_parser/snippet.h"
#include "components/sync_driver/device_info.h"
#include "components/sync_driver/device_info_tracker.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "grit/components_strings.h"
#include "sync/protocol/history_delete_directive_specifics.pb.h"
#include "sync/protocol/sync_enums.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#endif

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/chrome_application.h"
#endif

// The amount of time to wait for a response from the WebHistoryService.
static const int kWebHistoryTimeoutSeconds = 3;

using bookmarks::BookmarkModel;

namespace {

// Buckets for UMA histograms.
enum WebHistoryQueryBuckets {
  WEB_HISTORY_QUERY_FAILED = 0,
  WEB_HISTORY_QUERY_SUCCEEDED,
  WEB_HISTORY_QUERY_TIMED_OUT,
  NUM_WEB_HISTORY_QUERY_BUCKETS
};

// Identifiers for the type of device from which a history entry originated.
static const char kDeviceTypeLaptop[] = "laptop";
static const char kDeviceTypePhone[] = "phone";
static const char kDeviceTypeTablet[] = "tablet";

// Returns a localized version of |visit_time| including a relative
// indicator (e.g. today, yesterday).
base::string16 GetRelativeDateLocalized(const base::Time& visit_time) {
  base::Time midnight = base::Time::Now().LocalMidnight();
  base::string16 date_str = ui::TimeFormat::RelativeDate(visit_time, &midnight);
  if (date_str.empty()) {
    date_str = base::TimeFormatFriendlyDate(visit_time);
  } else {
    date_str = l10n_util::GetStringFUTF16(
        IDS_HISTORY_DATE_WITH_RELATIVE_TIME,
        date_str,
        base::TimeFormatFriendlyDate(visit_time));
  }
  return date_str;
}

// Sets the correct year when substracting months from a date.
void NormalizeMonths(base::Time::Exploded* exploded) {
  // Decrease a year at a time until we have a proper date.
  while (exploded->month < 1) {
    exploded->month += 12;
    exploded->year--;
  }
}

// Returns true if |entry| represents a local visit that had no corresponding
// visit on the server.
bool IsLocalOnlyResult(const BrowsingHistoryHandler::HistoryEntry& entry) {
  return entry.entry_type == BrowsingHistoryHandler::HistoryEntry::LOCAL_ENTRY;
}

// Gets the name and type of a device for the given sync client ID.
// |name| and |type| are out parameters.
void GetDeviceNameAndType(const ProfileSyncService* sync_service,
                          const std::string& client_id,
                          std::string* name,
                          std::string* type) {
  // DeviceInfoTracker must be syncing in order for remote history entries to
  // be available.
  DCHECK(sync_service);
  DCHECK(sync_service->GetDeviceInfoTracker());
  DCHECK(sync_service->GetDeviceInfoTracker()->IsSyncing());

  scoped_ptr<sync_driver::DeviceInfo> device_info =
      sync_service->GetDeviceInfoTracker()->GetDeviceInfo(client_id);
  if (device_info.get()) {
    *name = device_info->client_name();
    switch (device_info->device_type()) {
      case sync_pb::SyncEnums::TYPE_PHONE:
        *type = kDeviceTypePhone;
        break;
      case sync_pb::SyncEnums::TYPE_TABLET:
        *type = kDeviceTypeTablet;
        break;
      default:
        *type = kDeviceTypeLaptop;
    }
    return;
  }

  *name = l10n_util::GetStringUTF8(IDS_HISTORY_UNKNOWN_DEVICE);
  *type = kDeviceTypeLaptop;
}

}  // namespace

BrowsingHistoryHandler::HistoryEntry::HistoryEntry(
    BrowsingHistoryHandler::HistoryEntry::EntryType entry_type,
    const GURL& url, const base::string16& title, base::Time time,
    const std::string& client_id, bool is_search_result,
    const base::string16& snippet, bool blocked_visit,
    const std::string& accept_languages) {
  this->entry_type = entry_type;
  this->url = url;
  this->title = title;
  this->time = time;
  this->client_id = client_id;
  all_timestamps.insert(time.ToInternalValue());
  this->is_search_result = is_search_result;
  this->snippet = snippet;
  this->blocked_visit = blocked_visit;
  this->accept_languages = accept_languages;
}

BrowsingHistoryHandler::HistoryEntry::HistoryEntry()
    : entry_type(EMPTY_ENTRY), is_search_result(false), blocked_visit(false) {
}

BrowsingHistoryHandler::HistoryEntry::~HistoryEntry() {
}

void BrowsingHistoryHandler::HistoryEntry::SetUrlAndTitle(
    base::DictionaryValue* result) const {
  result->SetString("url", url.spec());

  bool using_url_as_the_title = false;
  base::string16 title_to_set(title);
  if (title.empty()) {
    using_url_as_the_title = true;
    title_to_set = base::UTF8ToUTF16(url.spec());
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

scoped_ptr<base::DictionaryValue> BrowsingHistoryHandler::HistoryEntry::ToValue(
    BookmarkModel* bookmark_model,
    SupervisedUserService* supervised_user_service,
    const ProfileSyncService* sync_service) const {
  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  SetUrlAndTitle(result.get());

  base::string16 domain =
      url_formatter::IDNToUnicode(url.host(), accept_languages);
  // When the domain is empty, use the scheme instead. This allows for a
  // sensible treatment of e.g. file: URLs when group by domain is on.
  if (domain.empty())
    domain = base::UTF8ToUTF16(url.scheme() + ":");

  // The items which are to be written into result are also described in
  // chrome/browser/resources/history/history.js in @typedef for
  // HistoryEntry. Please update it whenever you add or remove
  // any keys in result.
  result->SetString("domain", domain);
  result->SetDouble("time", time.ToJsTime());

  // Pass the timestamps in a list.
  scoped_ptr<base::ListValue> timestamps(new base::ListValue);
  for (std::set<int64_t>::const_iterator it = all_timestamps.begin();
       it != all_timestamps.end(); ++it) {
    timestamps->AppendDouble(base::Time::FromInternalValue(*it).ToJsTime());
  }
  result->Set("allTimestamps", timestamps.release());

  // Always pass the short date since it is needed both in the search and in
  // the monthly view.
  result->SetString("dateShort", base::TimeFormatShortDate(time));

  base::string16 snippet_string;
  base::string16 date_relative_day;
  base::string16 date_time_of_day;
  bool is_blocked_visit = false;
  int host_filtering_behavior = -1;

  // Only pass in the strings we need (search results need a shortdate
  // and snippet, browse results need day and time information). Makes sure that
  // values of result are never undefined
  if (is_search_result) {
    snippet_string = snippet;
  } else {
    base::Time midnight = base::Time::Now().LocalMidnight();
    base::string16 date_str = ui::TimeFormat::RelativeDate(time, &midnight);
    if (date_str.empty()) {
      date_str = base::TimeFormatFriendlyDate(time);
    } else {
      date_str = l10n_util::GetStringFUTF16(
          IDS_HISTORY_DATE_WITH_RELATIVE_TIME,
          date_str,
          base::TimeFormatFriendlyDate(time));
    }
    date_relative_day = date_str;
    date_time_of_day = base::TimeFormatTimeOfDay(time);
  }

  std::string device_name;
  std::string device_type;
  if (!client_id.empty())
    GetDeviceNameAndType(sync_service, client_id, &device_name, &device_type);
  result->SetString("deviceName", device_name);
  result->SetString("deviceType", device_type);

#if defined(ENABLE_SUPERVISED_USERS)
  if (supervised_user_service) {
    const SupervisedUserURLFilter* url_filter =
        supervised_user_service->GetURLFilterForUIThread();
    int filtering_behavior =
        url_filter->GetFilteringBehaviorForURL(url.GetWithEmptyPath());
    is_blocked_visit = blocked_visit;
    host_filtering_behavior = filtering_behavior;
  }
#endif

  result->SetString("dateTimeOfDay", date_time_of_day);
  result->SetString("dateRelativeDay", date_relative_day);
  result->SetString("snippet", snippet_string);
  result->SetBoolean("starred", bookmark_model->IsBookmarked(url));
  result->SetInteger("hostFilteringBehavior", host_filtering_behavior);
  result->SetBoolean("blockedVisit", is_blocked_visit);

  return result;
}

bool BrowsingHistoryHandler::HistoryEntry::SortByTimeDescending(
    const BrowsingHistoryHandler::HistoryEntry& entry1,
    const BrowsingHistoryHandler::HistoryEntry& entry2) {
  return entry1.time > entry2.time;
}

BrowsingHistoryHandler::BrowsingHistoryHandler()
    : has_pending_delete_request_(false),
      history_service_observer_(this),
      weak_factory_(this) {
}

BrowsingHistoryHandler::~BrowsingHistoryHandler() {
  query_task_tracker_.TryCancelAll();
  web_history_request_.reset();
}

void BrowsingHistoryHandler::RegisterMessages() {
  // Create our favicon data source.
  Profile* profile = Profile::FromWebUI(web_ui());
  content::URLDataSource::Add(
      profile, new FaviconSource(profile, FaviconSource::ANY));

  // Get notifications when history is cleared.
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (hs)
    history_service_observer_.Add(hs);

  web_ui()->RegisterMessageCallback("queryHistory",
      base::Bind(&BrowsingHistoryHandler::HandleQueryHistory,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeVisits",
      base::Bind(&BrowsingHistoryHandler::HandleRemoveVisits,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("clearBrowsingData",
      base::Bind(&BrowsingHistoryHandler::HandleClearBrowsingData,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeBookmark",
      base::Bind(&BrowsingHistoryHandler::HandleRemoveBookmark,
                 base::Unretained(this)));
}

bool BrowsingHistoryHandler::ExtractIntegerValueAtIndex(
    const base::ListValue* value,
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
  if (!query_task_tracker_.HasTrackedTasks())
    ReturnResultsToFrontEnd();

  UMA_HISTOGRAM_ENUMERATION(
      "WebHistory.QueryCompletion",
      WEB_HISTORY_QUERY_TIMED_OUT, NUM_WEB_HISTORY_QUERY_BUCKETS);
}

void BrowsingHistoryHandler::QueryHistory(
    const base::string16& search_text,
    const history::QueryOptions& options) {
  Profile* profile = Profile::FromWebUI(web_ui());

  // Anything in-flight is invalid.
  query_task_tracker_.TryCancelAll();
  web_history_request_.reset();

  query_results_.clear();
  results_info_value_.Clear();

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text,
                   options,
                   base::Bind(&BrowsingHistoryHandler::QueryComplete,
                              base::Unretained(this),
                              search_text,
                              options),
                   &query_task_tracker_);

  history::WebHistoryService* web_history =
      WebHistoryServiceFactory::GetForProfile(profile);

  // Set this to false until the results actually arrive.
  results_info_value_.SetBoolean("hasSyncedResults", false);

  if (web_history) {
    web_history_query_results_.clear();
    web_history_request_ = web_history->QueryHistory(
        search_text,
        options,
        base::Bind(&BrowsingHistoryHandler::WebHistoryQueryComplete,
                   base::Unretained(this),
                   search_text, options,
                   base::TimeTicks::Now()));
    // Start a timer so we know when to give up.
    web_history_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kWebHistoryTimeoutSeconds),
        this, &BrowsingHistoryHandler::WebHistoryTimeout);
  }
}

void BrowsingHistoryHandler::HandleQueryHistory(const base::ListValue* args) {
  history::QueryOptions options;

  // Parse the arguments from JavaScript. There are five required arguments:
  // - the text to search for (may be empty)
  // - the offset from which the search should start (in multiples of week or
  //   month, set by the next argument).
  // - the range (BrowsingHistoryHandler::Range) Enum value that sets the range
  //   of the query.
  // - the end time for the query. Only results older than this time will be
  //   returned.
  // - the maximum number of results to return (may be 0, meaning that there
  //   is no maximum).
  base::string16 search_text = ExtractStringValue(args);
  int offset;
  if (!args->GetInteger(1, &offset)) {
    NOTREACHED() << "Failed to convert argument 1. ";
    return;
  }
  int range;
  if (!args->GetInteger(2, &range)) {
    NOTREACHED() << "Failed to convert argument 2. ";
    return;
  }

  if (range == BrowsingHistoryHandler::MONTH)
    SetQueryTimeInMonths(offset, &options);
  else if (range == BrowsingHistoryHandler::WEEK)
    SetQueryTimeInWeeks(offset, &options);

  double end_time;
  if (!args->GetDouble(3, &end_time)) {
    NOTREACHED() << "Failed to convert argument 3. ";
    return;
  }
  if (end_time)
    options.end_time = base::Time::FromJsTime(end_time);

  if (!ExtractIntegerValueAtIndex(args, 4, &options.max_count)) {
    NOTREACHED() << "Failed to convert argument 4.";
    return;
  }

  options.duplicate_policy = history::QueryOptions::REMOVE_DUPLICATES_PER_DAY;
  QueryHistory(search_text, options);
}

void BrowsingHistoryHandler::HandleRemoveVisits(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  // TODO(davidben): history.js is not aware of this failure and will still
  // override |deleteCompleteCallback_|.
  if (delete_task_tracker_.HasTrackedTasks() ||
      has_pending_delete_request_ ||
      !profile->GetPrefs()->GetBoolean(prefs::kAllowDeletingBrowserHistory)) {
    web_ui()->CallJavascriptFunction("deleteFailed");
    return;
  }

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::WebHistoryService* web_history =
      WebHistoryServiceFactory::GetForProfile(profile);

  base::Time now = base::Time::Now();
  std::vector<history::ExpireHistoryArgs> expire_list;
  expire_list.reserve(args->GetSize());

  DCHECK(urls_to_be_deleted_.empty());
  for (base::ListValue::const_iterator it = args->begin();
       it != args->end(); ++it) {
    base::DictionaryValue* deletion = NULL;
    base::string16 url;
    base::ListValue* timestamps = NULL;

    // Each argument is a dictionary with properties "url" and "timestamps".
    if (!((*it)->GetAsDictionary(&deletion) &&
        deletion->GetString("url", &url) &&
        deletion->GetList("timestamps", &timestamps))) {
      NOTREACHED() << "Unable to extract arguments";
      return;
    }
    DCHECK(timestamps->GetSize() > 0);

    // In order to ensure that visits will be deleted from the server and other
    // clients (even if they are offline), create a sync delete directive for
    // each visit to be deleted.
    sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;
    sync_pb::GlobalIdDirective* global_id_directive =
        delete_directive.mutable_global_id_directive();

    double timestamp;
    history::ExpireHistoryArgs* expire_args = NULL;
    for (base::ListValue::const_iterator ts_iterator = timestamps->begin();
         ts_iterator != timestamps->end(); ++ts_iterator) {
      if (!(*ts_iterator)->GetAsDouble(&timestamp)) {
        NOTREACHED() << "Unable to extract visit timestamp.";
        continue;
      }
      base::Time visit_time = base::Time::FromJsTime(timestamp);
      if (!expire_args) {
        GURL gurl(url);
        expire_list.resize(expire_list.size() + 1);
        expire_args = &expire_list.back();
        expire_args->SetTimeRangeForOneDay(visit_time);
        expire_args->urls.insert(gurl);
        urls_to_be_deleted_.insert(gurl);
      }
      // The local visit time is treated as a global ID for the visit.
      global_id_directive->add_global_id(visit_time.ToInternalValue());
    }

    // Set the start and end time in microseconds since the Unix epoch.
    global_id_directive->set_start_time_usec(
        (expire_args->begin_time - base::Time::UnixEpoch()).InMicroseconds());

    // Delete directives shouldn't have an end time in the future.
    // TODO(dubroy): Use sane time (crbug.com/146090) here when it's ready.
    base::Time end_time = std::min(expire_args->end_time, now);

    // -1 because end time in delete directives is inclusive.
    global_id_directive->set_end_time_usec(
        (end_time - base::Time::UnixEpoch()).InMicroseconds() - 1);

    // TODO(dubroy): Figure out the proper way to handle an error here.
    if (web_history)
      history_service->ProcessLocalDeleteDirective(delete_directive);
  }

  history_service->ExpireHistory(
      expire_list,
      base::Bind(&BrowsingHistoryHandler::RemoveComplete,
                 base::Unretained(this)),
      &delete_task_tracker_);

  if (web_history) {
    has_pending_delete_request_ = true;
    web_history->ExpireHistory(
        expire_list,
        base::Bind(&BrowsingHistoryHandler::RemoveWebHistoryComplete,
                   weak_factory_.GetWeakPtr()));
  }

#if defined(ENABLE_EXTENSIONS)
  // If the profile has activity logging enabled also clean up any URLs from
  // the extension activity log. The extension activity log contains URLS
  // which websites an extension has activity on so it will indirectly
  // contain websites that a user has visited.
  extensions::ActivityLog* activity_log =
      extensions::ActivityLog::GetInstance(profile);
  for (std::vector<history::ExpireHistoryArgs>::const_iterator it =
       expire_list.begin(); it != expire_list.end(); ++it) {
    activity_log->RemoveURLs(it->urls);
  }
#endif

  for (const history::ExpireHistoryArgs& expire_entry : expire_list)
    AppBannerSettingsHelper::ClearHistoryForURLs(profile, expire_entry.urls);
}

void BrowsingHistoryHandler::HandleClearBrowsingData(
    const base::ListValue* args) {
#if BUILDFLAG(ANDROID_JAVA_UI)
  chrome::android::ChromeApplication::OpenClearBrowsingData(
      web_ui()->GetWebContents());
#else
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  chrome::ShowClearBrowsingDataDialog(browser);
#endif
}

void BrowsingHistoryHandler::HandleRemoveBookmark(const base::ListValue* args) {
  base::string16 url = ExtractStringValue(args);
  Profile* profile = Profile::FromWebUI(web_ui());
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile);
  bookmarks::RemoveAllBookmarks(model, GURL(url));
}

// static
void BrowsingHistoryHandler::MergeDuplicateResults(
    std::vector<BrowsingHistoryHandler::HistoryEntry>* results) {
  std::vector<BrowsingHistoryHandler::HistoryEntry> new_results;
  // Pre-reserve the size of the new vector. Since we're working with pointers
  // later on not doing this could lead to the vector being resized and to
  // pointers to invalid locations.
  new_results.reserve(results->size());
  // Maps a URL to the most recent entry on a particular day.
  std::map<GURL, BrowsingHistoryHandler::HistoryEntry*> current_day_entries;

  // Keeps track of the day that |current_day_urls| is holding the URLs for,
  // in order to handle removing per-day duplicates.
  base::Time current_day_midnight;

  std::sort(
      results->begin(), results->end(), HistoryEntry::SortByTimeDescending);

  for (std::vector<BrowsingHistoryHandler::HistoryEntry>::const_iterator it =
           results->begin(); it != results->end(); ++it) {
    // Reset the list of found URLs when a visit from a new day is encountered.
    if (current_day_midnight != it->time.LocalMidnight()) {
      current_day_entries.clear();
      current_day_midnight = it->time.LocalMidnight();
    }

    // Keep this visit if it's the first visit to this URL on the current day.
    if (current_day_entries.count(it->url) == 0) {
      new_results.push_back(*it);
      current_day_entries[it->url] = &new_results.back();
    } else {
      // Keep track of the timestamps of all visits to the URL on the same day.
      BrowsingHistoryHandler::HistoryEntry* entry =
          current_day_entries[it->url];
      entry->all_timestamps.insert(
          it->all_timestamps.begin(), it->all_timestamps.end());

      if (entry->entry_type != it->entry_type) {
        entry->entry_type =
            BrowsingHistoryHandler::HistoryEntry::COMBINED_ENTRY;
      }
    }
  }
  results->swap(new_results);
}

void BrowsingHistoryHandler::ReturnResultsToFrontEnd() {
  Profile* profile = Profile::FromWebUI(web_ui());
  BookmarkModel* bookmark_model = BookmarkModelFactory::GetForProfile(profile);
  SupervisedUserService* supervised_user_service = NULL;
#if defined(ENABLE_SUPERVISED_USERS)
  if (profile->IsSupervised())
    supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile);
#endif
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);

  // Combine the local and remote results into |query_results_|, and remove
  // any duplicates.
  if (!web_history_query_results_.empty()) {
    int local_result_count = query_results_.size();
    query_results_.insert(query_results_.end(),
                          web_history_query_results_.begin(),
                          web_history_query_results_.end());
    MergeDuplicateResults(&query_results_);

    if (local_result_count) {
      // In the best case, we expect that all local results are duplicated on
      // the server. Keep track of how many are missing.
      int missing_count = std::count_if(
          query_results_.begin(), query_results_.end(), IsLocalOnlyResult);
      UMA_HISTOGRAM_PERCENTAGE("WebHistory.LocalResultMissingOnServer",
                               missing_count * 100.0 / local_result_count);
    }
  }

  // Convert the result vector into a ListValue.
  base::ListValue results_value;
  for (std::vector<BrowsingHistoryHandler::HistoryEntry>::iterator it =
           query_results_.begin(); it != query_results_.end(); ++it) {
    scoped_ptr<base::Value> value(
        it->ToValue(bookmark_model, supervised_user_service, sync_service));
    results_value.Append(value.release());
  }

  web_ui()->CallJavascriptFunction(
      "historyResult", results_info_value_, results_value);
  results_info_value_.Clear();
  query_results_.clear();
  web_history_query_results_.clear();
}

void BrowsingHistoryHandler::QueryComplete(
    const base::string16& search_text,
    const history::QueryOptions& options,
    history::QueryResults* results) {
  DCHECK_EQ(0U, query_results_.size());
  query_results_.reserve(results->size());
  const std::string accept_languages = GetAcceptLanguages();

  for (size_t i = 0; i < results->size(); ++i) {
    history::URLResult const &page = (*results)[i];
    // TODO(dubroy): Use sane time (crbug.com/146090) here when it's ready.
    query_results_.push_back(
        HistoryEntry(
            HistoryEntry::LOCAL_ENTRY,
            page.url(),
            page.title(),
            page.visit_time(),
            std::string(),
            !search_text.empty(),
            page.snippet().text(),
            page.blocked_visit(),
            accept_languages));
  }

  // The items which are to be written into results_info_value_ are also
  // described in chrome/browser/resources/history/history.js in @typedef for
  // HistoryQuery. Please update it whenever you add or remove any keys in
  // results_info_value_.
  results_info_value_.SetString("term", search_text);
  results_info_value_.SetBoolean("finished", results->reached_beginning());

  // Add the specific dates that were searched to display them.
  // TODO(sergiu): Put today if the start is in the future.
  results_info_value_.SetString("queryStartTime",
                                GetRelativeDateLocalized(options.begin_time));
  if (!options.end_time.is_null()) {
    results_info_value_.SetString("queryEndTime",
        GetRelativeDateLocalized(options.end_time -
                                 base::TimeDelta::FromDays(1)));
  } else {
    results_info_value_.SetString("queryEndTime",
        GetRelativeDateLocalized(base::Time::Now()));
  }
  if (!web_history_timer_.IsRunning())
    ReturnResultsToFrontEnd();
}

void BrowsingHistoryHandler::WebHistoryQueryComplete(
    const base::string16& search_text,
    const history::QueryOptions& options,
    base::TimeTicks start_time,
    history::WebHistoryService::Request* request,
    const base::DictionaryValue* results_value) {
  base::TimeDelta delta = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("WebHistory.ResponseTime", delta);
  const std::string accept_languages = GetAcceptLanguages();

  // If the response came in too late, do nothing.
  // TODO(dubroy): Maybe show a banner, and prompt the user to reload?
  if (!web_history_timer_.IsRunning())
    return;
  web_history_timer_.Stop();

  UMA_HISTOGRAM_ENUMERATION(
      "WebHistory.QueryCompletion",
      results_value ? WEB_HISTORY_QUERY_SUCCEEDED : WEB_HISTORY_QUERY_FAILED,
      NUM_WEB_HISTORY_QUERY_BUCKETS);

  DCHECK_EQ(0U, web_history_query_results_.size());
  const base::ListValue* events = NULL;
  if (results_value && results_value->GetList("event", &events)) {
    web_history_query_results_.reserve(events->GetSize());
    for (unsigned int i = 0; i < events->GetSize(); ++i) {
      const base::DictionaryValue* event = NULL;
      const base::DictionaryValue* result = NULL;
      const base::ListValue* results = NULL;
      const base::ListValue* ids = NULL;
      base::string16 url;
      base::string16 title;
      base::Time visit_time;

      if (!(events->GetDictionary(i, &event) &&
          event->GetList("result", &results) &&
          results->GetDictionary(0, &result) &&
          result->GetString("url", &url) &&
          result->GetList("id", &ids) &&
          ids->GetSize() > 0)) {
        LOG(WARNING) << "Improperly formed JSON response from history server.";
        continue;
      }

      // Ignore any URLs that should not be shown in the history page.
      GURL gurl(url);
      if (!CanAddURLToHistory(gurl))
        continue;

      // Title is optional, so the return value is ignored here.
      result->GetString("title", &title);

      // Extract the timestamps of all the visits to this URL.
      // They are referred to as "IDs" by the server.
      for (int j = 0; j < static_cast<int>(ids->GetSize()); ++j) {
        const base::DictionaryValue* id = NULL;
        std::string timestamp_string;
        int64_t timestamp_usec = 0;

        if (!ids->GetDictionary(j, &id) ||
            !id->GetString("timestamp_usec", &timestamp_string) ||
            !base::StringToInt64(timestamp_string, &timestamp_usec)) {
          NOTREACHED() << "Unable to extract timestamp.";
          continue;
        }
        // The timestamp on the server is a Unix time.
        base::Time time = base::Time::UnixEpoch() +
            base::TimeDelta::FromMicroseconds(timestamp_usec);

        // Get the ID of the client that this visit came from.
        std::string client_id;
        id->GetString("client_id", &client_id);

        web_history_query_results_.push_back(
            HistoryEntry(
                HistoryEntry::REMOTE_ENTRY,
                gurl,
                title,
                time,
                client_id,
                !search_text.empty(),
                base::string16(),
                /* blocked_visit */ false,
                accept_languages));
      }
    }
  }
  results_info_value_.SetBoolean("hasSyncedResults", results_value != NULL);
  if (!query_task_tracker_.HasTrackedTasks())
    ReturnResultsToFrontEnd();
}

void BrowsingHistoryHandler::RemoveComplete() {
  urls_to_be_deleted_.clear();

  // Notify the page that the deletion request is complete, but only if a web
  // history delete request is not still pending.
  if (!has_pending_delete_request_)
    web_ui()->CallJavascriptFunction("deleteComplete");
}

void BrowsingHistoryHandler::RemoveWebHistoryComplete(bool success) {
  has_pending_delete_request_ = false;
  // TODO(dubroy): Should we handle failure somehow? Delete directives will
  // ensure that the visits are eventually deleted, so maybe it's not necessary.
  if (!delete_task_tracker_.HasTrackedTasks())
    RemoveComplete();
}

void BrowsingHistoryHandler::SetQueryTimeInWeeks(
    int offset, history::QueryOptions* options) {
  // LocalMidnight returns the beginning of the current day so get the
  // beginning of the next one.
  base::Time midnight = base::Time::Now().LocalMidnight() +
                              base::TimeDelta::FromDays(1);
  options->end_time = midnight -
      base::TimeDelta::FromDays(7 * offset);
  options->begin_time = midnight -
      base::TimeDelta::FromDays(7 * (offset + 1));
}

void BrowsingHistoryHandler::SetQueryTimeInMonths(
    int offset, history::QueryOptions* options) {
  // Configure the begin point of the search to the start of the
  // current month.
  base::Time::Exploded exploded;
  base::Time::Now().LocalMidnight().LocalExplode(&exploded);
  exploded.day_of_month = 1;

  if (offset == 0) {
    options->begin_time = base::Time::FromLocalExploded(exploded);

    // Set the end time of this first search to null (which will
    // show results from the future, should the user's clock have
    // been set incorrectly).
    options->end_time = base::Time();
  } else {
    // Go back |offset| months in the past. The end time is not inclusive, so
    // use the first day of the |offset| - 1 and |offset| months (e.g. for
    // the last month, |offset| = 1, use the first days of the last month and
    // the current month.
    exploded.month -= offset - 1;
    // Set the correct year.
    NormalizeMonths(&exploded);
    options->end_time = base::Time::FromLocalExploded(exploded);

    exploded.month -= 1;
    // Set the correct year
    NormalizeMonths(&exploded);
    options->begin_time = base::Time::FromLocalExploded(exploded);
  }
}

// Helper function for Observe that determines if there are any differences
// between the URLs noticed for deletion and the ones we are expecting.
static bool DeletionsDiffer(const history::URLRows& deleted_rows,
                            const std::set<GURL>& urls_to_be_deleted) {
  if (deleted_rows.size() != urls_to_be_deleted.size())
    return true;
  for (const auto& i : deleted_rows) {
    if (urls_to_be_deleted.find(i.url()) == urls_to_be_deleted.end())
      return true;
  }
  return false;
}

std::string BrowsingHistoryHandler::GetAcceptLanguages() const {
  Profile* profile = Profile::FromWebUI(web_ui());
  return profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
}

void BrowsingHistoryHandler::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  if (all_history || DeletionsDiffer(deleted_rows, urls_to_be_deleted_))
    web_ui()->CallJavascriptFunction("historyDeleted");
}
