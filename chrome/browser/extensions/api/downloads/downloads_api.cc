// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/downloads/downloads_api.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/download/download_file_icon_extractor.h"
#include "chrome/browser/download/download_query.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/downloads.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/webui/web_ui_util.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadId;
using content::DownloadItem;
using content::DownloadManager;

namespace download_extension_errors {

// Error messages
const char kGenericError[] = "I'm afraid I can't do that.";
const char kIconNotFoundError[] = "Icon not found.";
const char kInvalidDangerTypeError[] = "Invalid danger type";
const char kInvalidFilterError[] = "Invalid query filter";
const char kInvalidOperationError[] = "Invalid operation.";
const char kInvalidOrderByError[] = "Invalid orderBy field";
const char kInvalidQueryLimit[] = "Invalid query limit";
const char kInvalidStateError[] = "Invalid state";
const char kInvalidURLError[] = "Invalid URL.";
const char kNotImplementedError[] = "NotImplemented.";

}  // namespace download_extension_errors

namespace {

// Default icon size for getFileIcon() in pixels.
const int  kDefaultIconSize = 32;

// Parameter keys
const char kBytesReceivedKey[] = "bytesReceived";
const char kDangerAcceptedKey[] = "dangerAccepted";
const char kDangerContent[] = "content";
const char kDangerFile[] = "file";
const char kDangerKey[] = "danger";
const char kDangerSafe[] = "safe";
const char kDangerUncommon[] = "uncommon";
const char kDangerAccepted[] = "accepted";
const char kDangerHost[] = "host";
const char kDangerUrl[] = "url";
const char kEndTimeKey[] = "endTime";
const char kEndedAfterKey[] = "endedAfter";
const char kEndedBeforeKey[] = "endedBefore";
const char kErrorKey[] = "error";
const char kExistsKey[] = "exists";
const char kFileSizeKey[] = "fileSize";
const char kFilenameKey[] = "filename";
const char kFilenameRegexKey[] = "filenameRegex";
const char kIdKey[] = "id";
const char kIncognito[] = "incognito";
const char kMimeKey[] = "mime";
const char kPausedKey[] = "paused";
const char kQueryKey[] = "query";
const char kStartTimeKey[] = "startTime";
const char kStartedAfterKey[] = "startedAfter";
const char kStartedBeforeKey[] = "startedBefore";
const char kStateComplete[] = "complete";
const char kStateInProgress[] = "in_progress";
const char kStateInterrupted[] = "interrupted";
const char kStateKey[] = "state";
const char kTotalBytesGreaterKey[] = "totalBytesGreater";
const char kTotalBytesKey[] = "totalBytes";
const char kTotalBytesLessKey[] = "totalBytesLess";
const char kUrlKey[] = "url";
const char kUrlRegexKey[] = "urlRegex";

// Note: Any change to the danger type strings, should be accompanied by a
// corresponding change to downloads.json.
const char* kDangerStrings[] = {
  kDangerSafe,
  kDangerFile,
  kDangerUrl,
  kDangerContent,
  kDangerSafe,
  kDangerUncommon,
  kDangerAccepted,
  kDangerHost,
};
COMPILE_ASSERT(arraysize(kDangerStrings) == content::DOWNLOAD_DANGER_TYPE_MAX,
               download_danger_type_enum_changed);

// Note: Any change to the state strings, should be accompanied by a
// corresponding change to downloads.json.
const char* kStateStrings[] = {
  kStateInProgress,
  kStateComplete,
  kStateInterrupted,
  kStateInterrupted,
};
COMPILE_ASSERT(arraysize(kStateStrings) == DownloadItem::MAX_DOWNLOAD_STATE,
               download_item_state_enum_changed);

const char* DangerString(content::DownloadDangerType danger) {
  DCHECK(danger >= 0);
  DCHECK(danger < static_cast<content::DownloadDangerType>(
      arraysize(kDangerStrings)));
  if (danger < 0 || danger >= static_cast<content::DownloadDangerType>(
      arraysize(kDangerStrings)))
    return "";
  return kDangerStrings[danger];
}

content::DownloadDangerType DangerEnumFromString(const std::string& danger) {
  for (size_t i = 0; i < arraysize(kDangerStrings); ++i) {
    if (danger == kDangerStrings[i])
      return static_cast<content::DownloadDangerType>(i);
  }
  return content::DOWNLOAD_DANGER_TYPE_MAX;
}

const char* StateString(DownloadItem::DownloadState state) {
  DCHECK(state >= 0);
  DCHECK(state < static_cast<DownloadItem::DownloadState>(
      arraysize(kStateStrings)));
  if (state < 0 || state >= static_cast<DownloadItem::DownloadState>(
      arraysize(kStateStrings)))
    return "";
  return kStateStrings[state];
}

DownloadItem::DownloadState StateEnumFromString(const std::string& state) {
  for (size_t i = 0; i < arraysize(kStateStrings); ++i) {
    if ((kStateStrings[i] != NULL) && (state == kStateStrings[i]))
      return static_cast<DownloadItem::DownloadState>(i);
  }
  return DownloadItem::MAX_DOWNLOAD_STATE;
}

bool ValidateFilename(const string16& filename) {
  // TODO(benjhayden): More robust validation of filename.
  if ((filename.find('/') != string16::npos) ||
      (filename.find('\\') != string16::npos))
    return false;
  if (filename.size() >= 2u && filename[0] == L'.' && filename[1] == L'.')
    return false;
  return true;
}

std::string TimeToISO8601(const base::Time& t) {
  base::Time::Exploded exploded;
  t.UTCExplode(&exploded);
  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second,
      exploded.millisecond);
}

scoped_ptr<base::DictionaryValue> DownloadItemToJSON(
    DownloadItem* download_item,
    bool incognito) {
  base::DictionaryValue* json = new base::DictionaryValue();
  json->SetBoolean(kExistsKey, !download_item->GetFileExternallyRemoved());
  json->SetInteger(kIdKey, download_item->GetId());
  json->SetString(kUrlKey, download_item->GetOriginalUrl().spec());
  json->SetString(
      kFilenameKey, download_item->GetFullPath().LossyDisplayName());
  json->SetString(kDangerKey, DangerString(download_item->GetDangerType()));
  if (download_item->GetDangerType() !=
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)
    json->SetBoolean(kDangerAcceptedKey,
                     download_item->GetDangerType() ==
                     content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED);
  json->SetString(kStateKey, StateString(download_item->GetState()));
  json->SetBoolean(kPausedKey, download_item->IsPaused());
  json->SetString(kMimeKey, download_item->GetMimeType());
  json->SetString(kStartTimeKey, TimeToISO8601(download_item->GetStartTime()));
  json->SetInteger(kBytesReceivedKey, download_item->GetReceivedBytes());
  json->SetInteger(kTotalBytesKey, download_item->GetTotalBytes());
  json->SetBoolean(kIncognito, incognito);
  if (download_item->GetState() == DownloadItem::INTERRUPTED) {
    json->SetInteger(kErrorKey, static_cast<int>(
        download_item->GetLastReason()));
  } else if (download_item->GetState() == DownloadItem::CANCELLED) {
    json->SetInteger(kErrorKey, static_cast<int>(
        content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED));
  }
  if (!download_item->GetEndTime().is_null())
    json->SetString(kEndTimeKey, TimeToISO8601(download_item->GetEndTime()));
  // TODO(benjhayden): Implement fileSize.
  json->SetInteger(kFileSizeKey, download_item->GetTotalBytes());
  return scoped_ptr<base::DictionaryValue>(json);
}

class DownloadFileIconExtractorImpl : public DownloadFileIconExtractor {
 public:
  DownloadFileIconExtractorImpl() {}

  virtual ~DownloadFileIconExtractorImpl() {}

  virtual bool ExtractIconURLForPath(const base::FilePath& path,
                                     IconLoader::IconSize icon_size,
                                     IconURLCallback callback) OVERRIDE;
 private:
  void OnIconLoadComplete(gfx::Image* icon);

  CancelableTaskTracker cancelable_task_tracker_;
  IconURLCallback callback_;
};

bool DownloadFileIconExtractorImpl::ExtractIconURLForPath(
    const base::FilePath& path,
    IconLoader::IconSize icon_size,
    IconURLCallback callback) {
  callback_ = callback;
  IconManager* im = g_browser_process->icon_manager();
  // The contents of the file at |path| may have changed since a previous
  // request, in which case the associated icon may also have changed.
  // Therefore, for the moment we always call LoadIcon instead of attempting
  // a LookupIcon.
  im->LoadIcon(path,
               icon_size,
               base::Bind(&DownloadFileIconExtractorImpl::OnIconLoadComplete,
                          base::Unretained(this)),
               &cancelable_task_tracker_);
  return true;
}

void DownloadFileIconExtractorImpl::OnIconLoadComplete(gfx::Image* icon) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string url;
  if (icon)
    url = webui::GetBitmapDataUrl(icon->AsBitmap());
  callback_.Run(url);
}

IconLoader::IconSize IconLoaderSizeFromPixelSize(int pixel_size) {
  switch (pixel_size) {
    case 16: return IconLoader::SMALL;
    case 32: return IconLoader::NORMAL;
    default:
      NOTREACHED();
      return IconLoader::NORMAL;
  }
}

typedef base::hash_map<std::string, DownloadQuery::FilterType> FilterTypeMap;

void InitFilterTypeMap(FilterTypeMap& filter_types) {
  filter_types[kBytesReceivedKey] = DownloadQuery::FILTER_BYTES_RECEIVED;
  filter_types[kDangerAcceptedKey] = DownloadQuery::FILTER_DANGER_ACCEPTED;
  filter_types[kExistsKey] = DownloadQuery::FILTER_EXISTS;
  filter_types[kFilenameKey] = DownloadQuery::FILTER_FILENAME;
  filter_types[kFilenameRegexKey] = DownloadQuery::FILTER_FILENAME_REGEX;
  filter_types[kMimeKey] = DownloadQuery::FILTER_MIME;
  filter_types[kPausedKey] = DownloadQuery::FILTER_PAUSED;
  filter_types[kQueryKey] = DownloadQuery::FILTER_QUERY;
  filter_types[kEndedAfterKey] = DownloadQuery::FILTER_ENDED_AFTER;
  filter_types[kEndedBeforeKey] = DownloadQuery::FILTER_ENDED_BEFORE;
  filter_types[kEndTimeKey] = DownloadQuery::FILTER_END_TIME;
  filter_types[kStartedAfterKey] = DownloadQuery::FILTER_STARTED_AFTER;
  filter_types[kStartedBeforeKey] = DownloadQuery::FILTER_STARTED_BEFORE;
  filter_types[kStartTimeKey] = DownloadQuery::FILTER_START_TIME;
  filter_types[kTotalBytesKey] = DownloadQuery::FILTER_TOTAL_BYTES;
  filter_types[kTotalBytesGreaterKey] =
    DownloadQuery::FILTER_TOTAL_BYTES_GREATER;
  filter_types[kTotalBytesLessKey] = DownloadQuery::FILTER_TOTAL_BYTES_LESS;
  filter_types[kUrlKey] = DownloadQuery::FILTER_URL;
  filter_types[kUrlRegexKey] = DownloadQuery::FILTER_URL_REGEX;
}

typedef base::hash_map<std::string, DownloadQuery::SortType> SortTypeMap;

void InitSortTypeMap(SortTypeMap& sorter_types) {
  sorter_types[kBytesReceivedKey] = DownloadQuery::SORT_BYTES_RECEIVED;
  sorter_types[kDangerKey] = DownloadQuery::SORT_DANGER;
  sorter_types[kDangerAcceptedKey] = DownloadQuery::SORT_DANGER_ACCEPTED;
  sorter_types[kEndTimeKey] = DownloadQuery::SORT_END_TIME;
  sorter_types[kExistsKey] = DownloadQuery::SORT_EXISTS;
  sorter_types[kFilenameKey] = DownloadQuery::SORT_FILENAME;
  sorter_types[kMimeKey] = DownloadQuery::SORT_MIME;
  sorter_types[kPausedKey] = DownloadQuery::SORT_PAUSED;
  sorter_types[kStartTimeKey] = DownloadQuery::SORT_START_TIME;
  sorter_types[kStateKey] = DownloadQuery::SORT_STATE;
  sorter_types[kTotalBytesKey] = DownloadQuery::SORT_TOTAL_BYTES;
  sorter_types[kUrlKey] = DownloadQuery::SORT_URL;
}

bool IsNotTemporaryDownloadFilter(const DownloadItem& download_item) {
  return !download_item.IsTemporary();
}

// Set |manager| to the on-record DownloadManager, and |incognito_manager| to
// the off-record DownloadManager if one exists and is requested via
// |include_incognito|. This should work regardless of whether |profile| is
// original or incognito.
void GetManagers(
    Profile* profile,
    bool include_incognito,
    DownloadManager** manager,
    DownloadManager** incognito_manager) {
  *manager = BrowserContext::GetDownloadManager(profile->GetOriginalProfile());
  if (profile->HasOffTheRecordProfile() &&
      (include_incognito ||
       profile->IsOffTheRecord())) {
    *incognito_manager = BrowserContext::GetDownloadManager(
        profile->GetOffTheRecordProfile());
  } else {
    *incognito_manager = NULL;
  }
}

DownloadItem* GetDownload(Profile* profile, bool include_incognito, int id) {
  DownloadManager* manager = NULL;
  DownloadManager* incognito_manager = NULL;
  GetManagers(profile, include_incognito, &manager, &incognito_manager);
  DownloadItem* download_item = manager->GetDownload(id);
  if (!download_item && incognito_manager)
    download_item = incognito_manager->GetDownload(id);
  return download_item;
}

DownloadItem* GetDownloadIfInProgress(
    Profile* profile,
    bool include_incognito,
    int id) {
  DownloadItem* download_item = GetDownload(profile, include_incognito, id);
  return download_item && download_item->IsInProgress() ? download_item : NULL;
}

enum DownloadsFunctionName {
  DOWNLOADS_FUNCTION_DOWNLOAD = 0,
  DOWNLOADS_FUNCTION_SEARCH = 1,
  DOWNLOADS_FUNCTION_PAUSE = 2,
  DOWNLOADS_FUNCTION_RESUME = 3,
  DOWNLOADS_FUNCTION_CANCEL = 4,
  DOWNLOADS_FUNCTION_ERASE = 5,
  DOWNLOADS_FUNCTION_SET_DESTINATION = 6,
  DOWNLOADS_FUNCTION_ACCEPT_DANGER = 7,
  DOWNLOADS_FUNCTION_SHOW = 8,
  DOWNLOADS_FUNCTION_DRAG = 9,
  DOWNLOADS_FUNCTION_GET_FILE_ICON = 10,
  DOWNLOADS_FUNCTION_OPEN = 11,
  // Insert new values here, not at the beginning.
  DOWNLOADS_FUNCTION_LAST
};

void RecordApiFunctions(DownloadsFunctionName function) {
  UMA_HISTOGRAM_ENUMERATION("Download.ApiFunctions",
                            function,
                            DOWNLOADS_FUNCTION_LAST);
}

void CompileDownloadQueryOrderBy(
    const std::string& order_by_str, std::string* error, DownloadQuery* query) {
  // TODO(benjhayden): Consider switching from LazyInstance to explicit string
  // comparisons.
  static base::LazyInstance<SortTypeMap> sorter_types =
    LAZY_INSTANCE_INITIALIZER;
  if (sorter_types.Get().size() == 0)
    InitSortTypeMap(sorter_types.Get());

  std::vector<std::string> order_by_strs;
  base::SplitString(order_by_str, ' ', &order_by_strs);
  for (std::vector<std::string>::const_iterator iter = order_by_strs.begin();
      iter != order_by_strs.end(); ++iter) {
    std::string term_str = *iter;
    if (term_str.empty())
      continue;
    DownloadQuery::SortDirection direction = DownloadQuery::ASCENDING;
    if (term_str[0] == '-') {
      direction = DownloadQuery::DESCENDING;
      term_str = term_str.substr(1);
    }
    SortTypeMap::const_iterator sorter_type =
        sorter_types.Get().find(term_str);
    if (sorter_type == sorter_types.Get().end()) {
      *error = download_extension_errors::kInvalidOrderByError;
      return;
    }
    query->AddSorter(sorter_type->second, direction);
  }
}

void RunDownloadQuery(
    const extensions::api::downloads::DownloadQuery& query_in,
    DownloadManager* manager,
    DownloadManager* incognito_manager,
    std::string* error,
    DownloadQuery::DownloadVector* results) {
  // TODO(benjhayden): Consider switching from LazyInstance to explicit string
  // comparisons.
  static base::LazyInstance<FilterTypeMap> filter_types =
    LAZY_INSTANCE_INITIALIZER;
  if (filter_types.Get().size() == 0)
    InitFilterTypeMap(filter_types.Get());

  DownloadQuery query_out;

  if (query_in.limit.get()) {
    if (*query_in.limit.get() < 0) {
      *error = download_extension_errors::kInvalidQueryLimit;
      return;
    }
    query_out.Limit(*query_in.limit.get());
  }
  std::string state_string =
      extensions::api::downloads::ToString(query_in.state);
  if (!state_string.empty()) {
    DownloadItem::DownloadState state = StateEnumFromString(state_string);
    if (state == DownloadItem::MAX_DOWNLOAD_STATE) {
      *error = download_extension_errors::kInvalidStateError;
      return;
    }
    query_out.AddFilter(state);
  }
  std::string danger_string =
      extensions::api::downloads::ToString(query_in.danger);
  if (!danger_string.empty()) {
    content::DownloadDangerType danger_type = DangerEnumFromString(
        danger_string);
    if (danger_type == content::DOWNLOAD_DANGER_TYPE_MAX) {
      *error = download_extension_errors::kInvalidDangerTypeError;
      return;
    }
    query_out.AddFilter(danger_type);
  }
  if (query_in.order_by.get()) {
    CompileDownloadQueryOrderBy(*query_in.order_by.get(), error, &query_out);
    if (!error->empty())
      return;
  }

  scoped_ptr<base::DictionaryValue> query_in_value(query_in.ToValue().Pass());
  for (base::DictionaryValue::Iterator query_json_field(*query_in_value.get());
       query_json_field.HasNext(); query_json_field.Advance()) {
    FilterTypeMap::const_iterator filter_type =
        filter_types.Get().find(query_json_field.key());
    if (filter_type != filter_types.Get().end()) {
      if (!query_out.AddFilter(filter_type->second, query_json_field.value())) {
        *error = download_extension_errors::kInvalidFilterError;
        return;
      }
    }
  }

  DownloadQuery::DownloadVector all_items;
  if (query_in.id.get()) {
    DownloadItem* download_item = manager->GetDownload(*query_in.id.get());
    if (!download_item && incognito_manager)
      download_item = incognito_manager->GetDownload(*query_in.id.get());
    if (download_item)
      all_items.push_back(download_item);
  } else {
    manager->GetAllDownloads(&all_items);
    if (incognito_manager)
      incognito_manager->GetAllDownloads(&all_items);
  }
  query_out.AddFilter(base::Bind(&IsNotTemporaryDownloadFilter));
  query_out.Search(all_items.begin(), all_items.end(), results);
}

void DispatchEventInternal(
    Profile* target_profile,
    const char* event_name,
    const std::string& json_args,
    scoped_ptr<base::ListValue> event_args) {
  if (!extensions::ExtensionSystem::Get(target_profile)->event_router())
    return;
  scoped_ptr<extensions::Event> event(new extensions::Event(
      event_name, event_args.Pass()));
  event->restrict_to_profile = target_profile;
  extensions::ExtensionSystem::Get(target_profile)->event_router()->
      BroadcastEvent(event.Pass());
  ExtensionDownloadsEventRouter::DownloadsNotificationSource
    notification_source;
  notification_source.event_name = event_name;
  notification_source.profile = target_profile;
  content::Source<ExtensionDownloadsEventRouter::DownloadsNotificationSource>
    content_source(&notification_source);
  std::string args_copy(json_args);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_DOWNLOADS_EVENT,
      content_source,
      content::Details<std::string>(&args_copy));
}

class ExtensionDownloadsEventRouterData : public base::SupportsUserData::Data {
 public:
  static ExtensionDownloadsEventRouterData* Get(DownloadItem* download_item) {
    base::SupportsUserData::Data* data = download_item->GetUserData(kKey);
    return (data == NULL) ? NULL :
        static_cast<ExtensionDownloadsEventRouterData*>(data);
  }

  explicit ExtensionDownloadsEventRouterData(
      DownloadItem* download_item,
      scoped_ptr<base::DictionaryValue> json_item)
      : updated_(0),
        changed_fired_(0),
        json_(json_item.Pass()) {
    download_item->SetUserData(kKey, this);
  }

  virtual ~ExtensionDownloadsEventRouterData() {
    if (updated_ > 0) {
      UMA_HISTOGRAM_PERCENTAGE("Download.OnChanged",
                               (changed_fired_ * 100 / updated_));
    }
  }

  const base::DictionaryValue& json() const { return *json_.get(); }
  void set_json(scoped_ptr<base::DictionaryValue> json_item) {
    json_ = json_item.Pass();
  }

  void OnItemUpdated() { ++updated_; }
  void OnChangedFired() { ++changed_fired_; }

 private:
  static const char kKey[];

  int updated_;
  int changed_fired_;
  scoped_ptr<base::DictionaryValue> json_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDownloadsEventRouterData);
};

const char ExtensionDownloadsEventRouterData::kKey[] =
  "DownloadItem ExtensionDownloadsEventRouterData";

class ManagerDestructionObserver : public DownloadManager::Observer {
 public:
  static void CheckForHistoryFilesRemoval(DownloadManager* manager) {
    if (!manager)
      return;
    if (!manager_file_existence_last_checked_)
      manager_file_existence_last_checked_ =
        new std::map<DownloadManager*, ManagerDestructionObserver*>();
    if (!(*manager_file_existence_last_checked_)[manager])
      (*manager_file_existence_last_checked_)[manager] =
        new ManagerDestructionObserver(manager);
    (*manager_file_existence_last_checked_)[manager]
      ->CheckForHistoryFilesRemovalInternal();
  }

 private:
  static const int kFileExistenceRateLimitSeconds = 10;

  explicit ManagerDestructionObserver(DownloadManager* manager)
      : manager_(manager) {
    manager_->AddObserver(this);
  }

  virtual ~ManagerDestructionObserver() {
    manager_->RemoveObserver(this);
  }

  virtual void ManagerGoingDown(DownloadManager* manager) OVERRIDE {
    manager_file_existence_last_checked_->erase(manager);
    if (manager_file_existence_last_checked_->size() == 0) {
      delete manager_file_existence_last_checked_;
      manager_file_existence_last_checked_ = NULL;
    }
  }

  void CheckForHistoryFilesRemovalInternal() {
    base::Time now(base::Time::Now());
    int delta = now.ToTimeT() - last_checked_.ToTimeT();
    if (delta > kFileExistenceRateLimitSeconds) {
      last_checked_ = now;
      manager_->CheckForHistoryFilesRemoval();
    }
  }

  static std::map<DownloadManager*, ManagerDestructionObserver*>*
    manager_file_existence_last_checked_;

  DownloadManager* manager_;
  base::Time last_checked_;

  DISALLOW_COPY_AND_ASSIGN(ManagerDestructionObserver);
};

std::map<DownloadManager*, ManagerDestructionObserver*>*
  ManagerDestructionObserver::manager_file_existence_last_checked_ = NULL;

}  // namespace

DownloadsDownloadFunction::DownloadsDownloadFunction() {}
DownloadsDownloadFunction::~DownloadsDownloadFunction() {}

bool DownloadsDownloadFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Download::Params> params(
      extensions::api::downloads::Download::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const extensions::api::downloads::DownloadOptions& options = params->options;
  GURL download_url(options.url);
  if (!download_url.is_valid() ||
      (!download_url.SchemeIs("data") &&
       download_url.GetOrigin() != GetExtension()->url().GetOrigin() &&
       !GetExtension()->HasHostPermission(download_url))) {
    error_ = download_extension_errors::kInvalidURLError;
    return false;
  }

  Profile* current_profile = profile();
  if (include_incognito() && profile()->HasOffTheRecordProfile())
    current_profile = profile()->GetOffTheRecordProfile();

  scoped_ptr<content::DownloadUrlParameters> download_params(
      new content::DownloadUrlParameters(
          download_url,
          render_view_host()->GetProcess()->GetID(),
          render_view_host()->GetRoutingID(),
          current_profile->GetResourceContext()));

  if (options.filename.get()) {
    // TODO(benjhayden): Make json_schema_compiler generate string16s instead of
    // std::strings. Can't get filename16 from options.ToValue() because that
    // converts it from std::string.
    base::DictionaryValue* options_value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options_value));
    string16 filename16;
    EXTENSION_FUNCTION_VALIDATE(options_value->GetString(
        kFilenameKey, &filename16));
    if (!ValidateFilename(filename16)) {
      error_ = download_extension_errors::kGenericError;
      return false;
    }
    // TODO(benjhayden) Ensure that this filename is interpreted as a path
    // relative to the default downloads directory without allowing '..'.
    download_params->set_suggested_name(filename16);
  }

  if (options.save_as.get())
    download_params->set_prompt(*options.save_as.get());

  if (options.headers.get()) {
    typedef extensions::api::downloads::HeaderNameValuePair HeaderNameValuePair;
    for (std::vector<linked_ptr<HeaderNameValuePair> >::const_iterator iter =
         options.headers->begin();
         iter != options.headers->end();
         ++iter) {
      const HeaderNameValuePair& name_value = **iter;
      if (!net::HttpUtil::IsSafeHeader(name_value.name)) {
        error_ = download_extension_errors::kGenericError;
        return false;
      }
      download_params->add_request_header(name_value.name, name_value.value);
    }
  }

  std::string method_string =
      extensions::api::downloads::ToString(options.method);
  if (!method_string.empty())
    download_params->set_method(method_string);
  if (options.body.get())
    download_params->set_post_body(*options.body.get());
  download_params->set_callback(base::Bind(
      &DownloadsDownloadFunction::OnStarted, this));
  // Prevent login prompts for 401/407 responses.
  download_params->set_load_flags(net::LOAD_DO_NOT_PROMPT_FOR_LOGIN);

  DownloadManager* manager = BrowserContext::GetDownloadManager(
      current_profile);
  manager->DownloadUrl(download_params.Pass());
  RecordApiFunctions(DOWNLOADS_FUNCTION_DOWNLOAD);
  return true;
}

void DownloadsDownloadFunction::OnStarted(
    DownloadItem* item, net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << __FUNCTION__ << " " << item << " " << error;
  if (item) {
    DCHECK_EQ(net::OK, error);
    SetResult(base::Value::CreateIntegerValue(item->GetId()));
  } else {
    DCHECK_NE(net::OK, error);
    error_ = net::ErrorToString(error);
  }
  SendResponse(error_.empty());
}

DownloadsSearchFunction::DownloadsSearchFunction() {}
DownloadsSearchFunction::~DownloadsSearchFunction() {}

bool DownloadsSearchFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Search::Params> params(
      extensions::api::downloads::Search::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadManager* manager = NULL;
  DownloadManager* incognito_manager = NULL;
  GetManagers(profile(), include_incognito(), &manager, &incognito_manager);
  ManagerDestructionObserver::CheckForHistoryFilesRemoval(manager);
  ManagerDestructionObserver::CheckForHistoryFilesRemoval(incognito_manager);
  DownloadQuery::DownloadVector results;
  RunDownloadQuery(params->query,
                   manager,
                   incognito_manager,
                   &error_,
                   &results);
  if (!error_.empty())
    return false;

  base::ListValue* json_results = new base::ListValue();
  for (DownloadManager::DownloadVector::const_iterator it = results.begin();
       it != results.end(); ++it) {
    DownloadItem* download_item = *it;
    int32 download_id = download_item->GetId();
    bool off_record = ((incognito_manager != NULL) &&
                       (incognito_manager->GetDownload(download_id) != NULL));
    scoped_ptr<base::DictionaryValue> json_item(DownloadItemToJSON(
        *it, off_record));
    json_results->Append(json_item.release());
  }
  SetResult(json_results);
  RecordApiFunctions(DOWNLOADS_FUNCTION_SEARCH);
  return true;
}

DownloadsPauseFunction::DownloadsPauseFunction() {}
DownloadsPauseFunction::~DownloadsPauseFunction() {}

bool DownloadsPauseFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Pause::Params> params(
      extensions::api::downloads::Pause::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadItem* download_item = GetDownloadIfInProgress(
      profile(), include_incognito(), params->download_id);
  if (download_item == NULL) {
    // This could be due to an invalid download ID, or it could be due to the
    // download not being currently active.
    error_ = download_extension_errors::kInvalidOperationError;
  } else {
    // If the item is already paused, this is a no-op and the
    // operation will silently succeed.
    download_item->Pause();
  }
  if (error_.empty())
    RecordApiFunctions(DOWNLOADS_FUNCTION_PAUSE);
  return error_.empty();
}

DownloadsResumeFunction::DownloadsResumeFunction() {}
DownloadsResumeFunction::~DownloadsResumeFunction() {}

bool DownloadsResumeFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Resume::Params> params(
      extensions::api::downloads::Resume::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadItem* download_item = GetDownloadIfInProgress(
      profile(), include_incognito(), params->download_id);
  if (download_item == NULL) {
    // This could be due to an invalid download ID, or it could be due to the
    // download not being currently active.
    error_ = download_extension_errors::kInvalidOperationError;
  } else {
    // Note that if the item isn't paused, this will be a no-op, and
    // we will silently treat the extension call as a success.
    download_item->Resume();
  }
  if (error_.empty())
    RecordApiFunctions(DOWNLOADS_FUNCTION_RESUME);
  return error_.empty();
}

DownloadsCancelFunction::DownloadsCancelFunction() {}
DownloadsCancelFunction::~DownloadsCancelFunction() {}

bool DownloadsCancelFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Resume::Params> params(
      extensions::api::downloads::Resume::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadItem* download_item = GetDownloadIfInProgress(
      profile(), include_incognito(), params->download_id);
  if (download_item != NULL)
    download_item->Cancel(true);
  // |download_item| can be NULL if the download ID was invalid or if the
  // download is not currently active.  Either way, we don't consider it a
  // failure.
  RecordApiFunctions(DOWNLOADS_FUNCTION_CANCEL);
  return true;
}

DownloadsEraseFunction::DownloadsEraseFunction() {}
DownloadsEraseFunction::~DownloadsEraseFunction() {}

bool DownloadsEraseFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Erase::Params> params(
      extensions::api::downloads::Erase::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadManager* manager = NULL;
  DownloadManager* incognito_manager = NULL;
  GetManagers(profile(), include_incognito(), &manager, &incognito_manager);
  DownloadQuery::DownloadVector results;
  RunDownloadQuery(params->query,
                   manager,
                   incognito_manager,
                   &error_,
                   &results);
  if (!error_.empty())
    return false;
  base::ListValue* json_results = new base::ListValue();
  for (DownloadManager::DownloadVector::const_iterator it = results.begin();
       it != results.end(); ++it) {
    json_results->Append(base::Value::CreateIntegerValue((*it)->GetId()));
    (*it)->Remove();
  }
  SetResult(json_results);
  RecordApiFunctions(DOWNLOADS_FUNCTION_ERASE);
  return true;
}

DownloadsSetDestinationFunction::DownloadsSetDestinationFunction() {}
DownloadsSetDestinationFunction::~DownloadsSetDestinationFunction() {}

bool DownloadsSetDestinationFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::SetDestination::Params> params(
      extensions::api::downloads::SetDestination::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  error_ = download_extension_errors::kNotImplementedError;
  if (error_.empty())
    RecordApiFunctions(DOWNLOADS_FUNCTION_SET_DESTINATION);
  return error_.empty();
}

DownloadsAcceptDangerFunction::DownloadsAcceptDangerFunction() {}
DownloadsAcceptDangerFunction::~DownloadsAcceptDangerFunction() {}

bool DownloadsAcceptDangerFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::AcceptDanger::Params> params(
      extensions::api::downloads::AcceptDanger::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadItem* download_item = GetDownloadIfInProgress(
      profile(), include_incognito(), params->download_id);
  content::WebContents* web_contents =
      dispatcher()->delegate()->GetAssociatedWebContents();
  if (!download_item ||
      !download_item->IsDangerous() ||
      !web_contents) {
    error_ = download_extension_errors::kInvalidOperationError;
    return false;
  }
  RecordApiFunctions(DOWNLOADS_FUNCTION_ACCEPT_DANGER);
  // DownloadDangerPrompt displays a modal dialog using native widgets that the
  // user must either accept or cancel. It cannot be scripted.
  DownloadDangerPrompt::Create(
      download_item,
      web_contents,
      true,
      base::Bind(&DownloadsAcceptDangerFunction::DangerPromptCallback,
                 this, true, params->download_id),
      base::Bind(&DownloadsAcceptDangerFunction::DangerPromptCallback,
                 this, false, params->download_id));
  // DownloadDangerPrompt deletes itself
  return true;
}

void DownloadsAcceptDangerFunction::DangerPromptCallback(
    bool accept, int download_id) {
  if (accept) {
    DownloadItem* download_item = GetDownloadIfInProgress(
        profile(), include_incognito(), download_id);
    if (download_item)
      download_item->DangerousDownloadValidated();
  }
  SendResponse(error_.empty());
}

DownloadsShowFunction::DownloadsShowFunction() {}
DownloadsShowFunction::~DownloadsShowFunction() {}

bool DownloadsShowFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Show::Params> params(
      extensions::api::downloads::Show::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadItem* download_item = GetDownload(
      profile(), include_incognito(), params->download_id);
  if (!download_item) {
    error_ = download_extension_errors::kInvalidOperationError;
    return false;
  }
  download_item->ShowDownloadInShell();
  RecordApiFunctions(DOWNLOADS_FUNCTION_SHOW);
  return true;
}

DownloadsOpenFunction::DownloadsOpenFunction() {}
DownloadsOpenFunction::~DownloadsOpenFunction() {}

bool DownloadsOpenFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Open::Params> params(
      extensions::api::downloads::Open::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadItem* download_item = GetDownload(
      profile(), include_incognito(), params->download_id);
  if (!download_item || !download_item->IsComplete()) {
    error_ = download_extension_errors::kInvalidOperationError;
    return false;
  }
  download_item->OpenDownload();
  RecordApiFunctions(DOWNLOADS_FUNCTION_OPEN);
  return true;
}

DownloadsDragFunction::DownloadsDragFunction() {}
DownloadsDragFunction::~DownloadsDragFunction() {}

bool DownloadsDragFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Drag::Params> params(
      extensions::api::downloads::Drag::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DownloadItem* download_item = GetDownload(
      profile(), include_incognito(), params->download_id);
  content::WebContents* web_contents =
      dispatcher()->delegate()->GetAssociatedWebContents();
  if (!download_item || !web_contents) {
    error_ = download_extension_errors::kInvalidOperationError;
    return false;
  }
  RecordApiFunctions(DOWNLOADS_FUNCTION_DRAG);
  gfx::Image* icon = g_browser_process->icon_manager()->LookupIcon(
      download_item->GetUserVerifiedFilePath(), IconLoader::NORMAL);
  gfx::NativeView view = web_contents->GetView()->GetNativeView();
  {
    // Enable nested tasks during DnD, while |DragDownload()| blocks.
    MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
    download_util::DragDownload(download_item, icon, view);
  }
  return true;
}

DownloadsGetFileIconFunction::DownloadsGetFileIconFunction()
    : icon_extractor_(new DownloadFileIconExtractorImpl()) {
}

DownloadsGetFileIconFunction::~DownloadsGetFileIconFunction() {}

void DownloadsGetFileIconFunction::SetIconExtractorForTesting(
    DownloadFileIconExtractor* extractor) {
  DCHECK(extractor);
  icon_extractor_.reset(extractor);
}

bool DownloadsGetFileIconFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::GetFileIcon::Params> params(
      extensions::api::downloads::GetFileIcon::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const extensions::api::downloads::GetFileIconOptions* options =
      params->options.get();
  int icon_size = kDefaultIconSize;
  if (options && options->size.get())
    icon_size = *options->size.get();
  DownloadItem* download_item = GetDownload(
      profile(), include_incognito(), params->download_id);
  if (!download_item || download_item->GetTargetFilePath().empty()) {
    error_ = download_extension_errors::kInvalidOperationError;
    return false;
  }
  // In-progress downloads return the intermediate filename for GetFullPath()
  // which doesn't have the final extension. Therefore we won't be able to
  // derive a good file icon for it. So we use GetTargetFilePath() instead.
  DCHECK(icon_extractor_.get());
  DCHECK(icon_size == 16 || icon_size == 32);
  EXTENSION_FUNCTION_VALIDATE(icon_extractor_->ExtractIconURLForPath(
      download_item->GetTargetFilePath(),
      IconLoaderSizeFromPixelSize(icon_size),
      base::Bind(&DownloadsGetFileIconFunction::OnIconURLExtracted, this)));
  return true;
}

void DownloadsGetFileIconFunction::OnIconURLExtracted(const std::string& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (url.empty()) {
    error_ = download_extension_errors::kIconNotFoundError;
  } else {
    RecordApiFunctions(DOWNLOADS_FUNCTION_GET_FILE_ICON);
    SetResult(base::Value::CreateStringValue(url));
  }
  SendResponse(error_.empty());
}

ExtensionDownloadsEventRouter::ExtensionDownloadsEventRouter(
    Profile* profile,
    DownloadManager* manager)
    : profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(notifier_(manager, this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
}

ExtensionDownloadsEventRouter::~ExtensionDownloadsEventRouter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ExtensionDownloadsEventRouter::OnDownloadCreated(
    DownloadManager* manager, DownloadItem* download_item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (download_item->IsTemporary())
    return;

  scoped_ptr<base::DictionaryValue> json_item(
      DownloadItemToJSON(download_item, profile_->IsOffTheRecord()));
  DispatchEvent(extensions::event_names::kOnDownloadCreated,
                json_item->DeepCopy());
  if (!ExtensionDownloadsEventRouterData::Get(download_item))
    new ExtensionDownloadsEventRouterData(download_item, json_item.Pass());
}

void ExtensionDownloadsEventRouter::OnDownloadUpdated(
    DownloadManager* manager, DownloadItem* download_item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (download_item->IsTemporary())
    return;

  ExtensionDownloadsEventRouterData* data =
    ExtensionDownloadsEventRouterData::Get(download_item);
  if (!data) {
    // The download_item probably transitioned from temporary to not temporary.
    OnDownloadCreated(manager, download_item);
    return;
  }
  scoped_ptr<base::DictionaryValue> new_json(DownloadItemToJSON(
        download_item, profile_->IsOffTheRecord()));
  scoped_ptr<base::DictionaryValue> delta(new base::DictionaryValue());
  delta->SetInteger(kIdKey, download_item->GetId());
  std::set<std::string> new_fields;
  bool changed = false;

  // For each field in the new json representation of the download_item except
  // the bytesReceived field, if the field has changed from the previous old
  // json, set the differences in the |delta| object and remember that something
  // significant changed.
  for (base::DictionaryValue::Iterator iter(*new_json.get());
       iter.HasNext(); iter.Advance()) {
    new_fields.insert(iter.key());
    if (iter.key() != kBytesReceivedKey) {
      const base::Value* old_value = NULL;
      if (!data->json().HasKey(iter.key()) ||
          (data->json().Get(iter.key(), &old_value) &&
           !iter.value().Equals(old_value))) {
        delta->Set(iter.key() + ".current", iter.value().DeepCopy());
        if (old_value)
          delta->Set(iter.key() + ".previous", old_value->DeepCopy());
        changed = true;
      }
    }
  }

  // If a field was in the previous json but is not in the new json, set the
  // difference in |delta|.
  for (base::DictionaryValue::Iterator iter(data->json());
       iter.HasNext(); iter.Advance()) {
    if (new_fields.find(iter.key()) == new_fields.end()) {
      delta->Set(iter.key() + ".previous", iter.value().DeepCopy());
      changed = true;
    }
  }

  // Update the OnChangedStat and dispatch the event if something significant
  // changed. Replace the stored json with the new json.
  data->OnItemUpdated();
  if (changed) {
    DispatchEvent(extensions::event_names::kOnDownloadChanged, delta.release());
    data->OnChangedFired();
  }
  data->set_json(new_json.Pass());
}

void ExtensionDownloadsEventRouter::OnDownloadRemoved(
    DownloadManager* manager, DownloadItem* download_item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (download_item->IsTemporary())
    return;
  DispatchEvent(extensions::event_names::kOnDownloadErased,
                base::Value::CreateIntegerValue(download_item->GetId()));
}

void ExtensionDownloadsEventRouter::DispatchEvent(
    const char* event_name, base::Value* arg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(arg);
  std::string json_args;
  base::JSONWriter::Write(args.get(), &json_args);
  // There is a one EDER for each on-record Profile, and a separate EDER for
  // each off-record Profile, so there is exactly one EDER for each
  // DownloadManager.  EDER only watches its own DM, so all the items that an
  // EDER sees are either all on-record or all off-record. However, we want
  // extensions in off-record contexts to see on-record items. So, if this EDER
  // is watching an on-record DM, and there is a corresponding off-record
  // Profile, then dispatch this event to both the on-record Profile and the
  // off-record Profile.  There may or may not be an off-record Profile, so send
  // a *copy* of |args| to the off-record Profile, and Pass() |args|
  // to the Profile that we know is there.
  if (profile_->HasOffTheRecordProfile() &&
      !profile_->IsOffTheRecord()) {
    DispatchEventInternal(
        profile_->GetOffTheRecordProfile(),
        event_name,
        json_args,
        scoped_ptr<base::ListValue>(args->DeepCopy()));
  }
  DispatchEventInternal(profile_, event_name, json_args, args.Pass());
}
