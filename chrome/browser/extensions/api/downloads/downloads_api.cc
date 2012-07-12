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
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_file_icon_extractor.h"
#include "chrome/browser/download/download_query.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
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
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

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
const char kBodyKey[] = "body";
const char kBytesReceivedKey[] = "bytesReceived";
const char kDangerAcceptedKey[] = "dangerAccepted";
const char kDangerContent[] = "content";
const char kDangerFile[] = "file";
const char kDangerKey[] = "danger";
const char kDangerSafe[] = "safe";
const char kDangerUncommon[] = "uncommon";
const char kDangerUrl[] = "url";
const char kEndTimeKey[] = "endTime";
const char kErrorKey[] = "error";
const char kFileSizeKey[] = "fileSize";
const char kFilenameKey[] = "filename";
const char kFilenameRegexKey[] = "filenameRegex";
const char kHeaderNameKey[] = "name";
const char kHeaderValueKey[] = "value";
const char kHeadersKey[] = "headers";
const char kIdKey[] = "id";
const char kIncognito[] = "incognito";
const char kLimitKey[] = "limit";
const char kMethodKey[] = "method";
const char kMimeKey[] = "mime";
const char kOrderByKey[] = "orderBy";
const char kPausedKey[] = "paused";
const char kQueryKey[] = "query";
const char kSaveAsKey[] = "saveAs";
const char kSizeKey[] = "size";
const char kStartTimeKey[] = "startTime";
const char kStartedAfterKey[] = "startedAfter";
const char kStartedBeforeKey[] = "startedBefore";
const char kStateComplete[] = "complete";
const char kStateInProgress[] = "in_progress";
const char kStateInterrupted[] = "interrupted";
const char kStateKey[] = "state";
const char kTotalBytesKey[] = "totalBytes";
const char kTotalBytesGreaterKey[] = "totalBytesGreater";
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
};
COMPILE_ASSERT(arraysize(kDangerStrings) == content::DOWNLOAD_DANGER_TYPE_MAX,
               download_danger_type_enum_changed);

// Note: Any change to the state strings, should be accompanied by a
// corresponding change to downloads.json.
const char* kStateStrings[] = {
  kStateInProgress,
  kStateComplete,
  kStateInterrupted,
  NULL,
  kStateInterrupted,
};
COMPILE_ASSERT(arraysize(kStateStrings) == DownloadItem::MAX_DOWNLOAD_STATE,
               download_item_state_enum_changed);

const char* DangerString(content::DownloadDangerType danger) {
  DCHECK(danger >= 0);
  DCHECK(danger < static_cast<content::DownloadDangerType>(
      arraysize(kDangerStrings)));
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
  DCHECK(state != DownloadItem::REMOVING);
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

scoped_ptr<base::DictionaryValue> DownloadItemToJSON(DownloadItem* item) {
  base::DictionaryValue* json = new base::DictionaryValue();
  json->SetInteger(kIdKey, item->GetId());
  json->SetString(kUrlKey, item->GetOriginalUrl().spec());
  json->SetString(kFilenameKey, item->GetFullPath().LossyDisplayName());
  json->SetString(kDangerKey, DangerString(item->GetDangerType()));
  if (item->GetDangerType() != content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)
    json->SetBoolean(kDangerAcceptedKey,
        item->GetSafetyState() == DownloadItem::DANGEROUS_BUT_VALIDATED);
  json->SetString(kStateKey, StateString(item->GetState()));
  json->SetBoolean(kPausedKey, item->IsPaused());
  json->SetString(kMimeKey, item->GetMimeType());
  json->SetInteger(kStartTimeKey,
      (item->GetStartTime() - base::Time::UnixEpoch()).InMilliseconds());
  json->SetInteger(kBytesReceivedKey, item->GetReceivedBytes());
  json->SetInteger(kTotalBytesKey, item->GetTotalBytes());
  json->SetBoolean(kIncognito, item->IsOtr());
  if (item->GetState() == DownloadItem::INTERRUPTED) {
    json->SetInteger(kErrorKey, static_cast<int>(item->GetLastReason()));
  } else if (item->GetState() == DownloadItem::CANCELLED) {
    json->SetInteger(kErrorKey, static_cast<int>(
        content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED));
  }
  // TODO(benjhayden): Implement endTime and fileSize.
  // json->SetInteger(kEndTimeKey, -1);
  json->SetInteger(kFileSizeKey, item->GetTotalBytes());
  return scoped_ptr<base::DictionaryValue>(json);
}

class DownloadFileIconExtractorImpl : public DownloadFileIconExtractor {
 public:
  DownloadFileIconExtractorImpl() {}

  ~DownloadFileIconExtractorImpl() {}

  virtual bool ExtractIconURLForPath(const FilePath& path,
                                     IconLoader::IconSize icon_size,
                                     IconURLCallback callback) OVERRIDE;
 private:
  void OnIconLoadComplete(IconManager::Handle handle, gfx::Image* icon);

  CancelableRequestConsumer cancelable_consumer_;
  IconURLCallback callback_;
};

bool DownloadFileIconExtractorImpl::ExtractIconURLForPath(
    const FilePath& path,
    IconLoader::IconSize icon_size,
    IconURLCallback callback) {
  callback_ = callback;
  IconManager* im = g_browser_process->icon_manager();
  // The contents of the file at |path| may have changed since a previous
  // request, in which case the associated icon may also have changed.
  // Therefore, for the moment we always call LoadIcon instead of attempting
  // a LookupIcon.
  im->LoadIcon(
      path, icon_size, &cancelable_consumer_,
      base::Bind(&DownloadFileIconExtractorImpl::OnIconLoadComplete,
                 base::Unretained(this)));
  return true;
}

void DownloadFileIconExtractorImpl::OnIconLoadComplete(
    IconManager::Handle handle,
    gfx::Image* icon) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string url;
  if (icon)
    url = web_ui_util::GetImageDataUrl(*icon->ToImageSkia());
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
  filter_types[kBytesReceivedKey] =
    DownloadQuery::FILTER_BYTES_RECEIVED;
  filter_types[kDangerAcceptedKey] =
    DownloadQuery::FILTER_DANGER_ACCEPTED;
  filter_types[kFilenameKey] = DownloadQuery::FILTER_FILENAME;
  filter_types[kFilenameRegexKey] =
    DownloadQuery::FILTER_FILENAME_REGEX;
  filter_types[kMimeKey] = DownloadQuery::FILTER_MIME;
  filter_types[kPausedKey] = DownloadQuery::FILTER_PAUSED;
  filter_types[kQueryKey] = DownloadQuery::FILTER_QUERY;
  filter_types[kStartedAfterKey] = DownloadQuery::FILTER_STARTED_AFTER;
  filter_types[kStartedBeforeKey] =
    DownloadQuery::FILTER_STARTED_BEFORE;
  filter_types[kStartTimeKey] = DownloadQuery::FILTER_START_TIME;
  filter_types[kTotalBytesKey] = DownloadQuery::FILTER_TOTAL_BYTES;
  filter_types[kTotalBytesGreaterKey] =
      DownloadQuery::FILTER_TOTAL_BYTES_GREATER;
  filter_types[kTotalBytesLessKey] =
    DownloadQuery::FILTER_TOTAL_BYTES_LESS;
  filter_types[kUrlKey] = DownloadQuery::FILTER_URL;
  filter_types[kUrlRegexKey] = DownloadQuery::FILTER_URL_REGEX;
}

typedef base::hash_map<std::string, DownloadQuery::SortType> SortTypeMap;

void InitSortTypeMap(SortTypeMap& sorter_types) {
  sorter_types[kBytesReceivedKey] = DownloadQuery::SORT_BYTES_RECEIVED;
  sorter_types[kDangerKey] = DownloadQuery::SORT_DANGER;
  sorter_types[kDangerAcceptedKey] =
    DownloadQuery::SORT_DANGER_ACCEPTED;
  sorter_types[kFilenameKey] = DownloadQuery::SORT_FILENAME;
  sorter_types[kMimeKey] = DownloadQuery::SORT_MIME;
  sorter_types[kPausedKey] = DownloadQuery::SORT_PAUSED;
  sorter_types[kStartTimeKey] = DownloadQuery::SORT_START_TIME;
  sorter_types[kStateKey] = DownloadQuery::SORT_STATE;
  sorter_types[kTotalBytesKey] = DownloadQuery::SORT_TOTAL_BYTES;
  sorter_types[kUrlKey] = DownloadQuery::SORT_URL;
}

bool IsNotTemporaryDownloadFilter(const DownloadItem& item) {
  return !item.IsTemporary();
}

void GetManagers(
    Profile* profile,
    bool include_incognito,
    DownloadManager** manager, DownloadManager** incognito_manager) {
  *manager = BrowserContext::GetDownloadManager(profile);
  *incognito_manager = NULL;
  if (include_incognito && profile->HasOffTheRecordProfile()) {
    *incognito_manager = BrowserContext::GetDownloadManager(
        profile->GetOffTheRecordProfile());
  }
}

DownloadItem* GetActiveItem(Profile* profile, bool include_incognito, int id) {
  DownloadManager* manager = NULL;
  DownloadManager* incognito_manager = NULL;
  GetManagers(profile, include_incognito, &manager, &incognito_manager);
  DownloadItem* download_item = manager->GetActiveDownloadItem(id);
  if (!download_item && incognito_manager)
    download_item = incognito_manager->GetActiveDownloadItem(id);
  return download_item;
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
    Profile* profile,
    bool include_incognito,
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
  if (query_in.state.get()) {
    DownloadItem::DownloadState state = StateEnumFromString(
        *query_in.state.get());
    if (state == DownloadItem::MAX_DOWNLOAD_STATE) {
      *error = download_extension_errors::kInvalidStateError;
      return;
    }
    query_out.AddFilter(state);
  }
  if (query_in.danger.get()) {
    content::DownloadDangerType danger_type =
        DangerEnumFromString(*query_in.danger.get());
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

  DownloadManager* manager = NULL;
  DownloadManager* incognito_manager = NULL;
  GetManagers(profile, include_incognito, &manager, &incognito_manager);
  DownloadQuery::DownloadVector all_items;
  if (query_in.id.get()) {
    DownloadItem* item = manager->GetDownloadItem(*query_in.id.get());
    if (!item && incognito_manager)
      item = incognito_manager->GetDownloadItem(*query_in.id.get());
    if (item)
      all_items.push_back(item);
  } else {
    manager->GetAllDownloads(FilePath(FILE_PATH_LITERAL("")), &all_items);
    if (incognito_manager)
      incognito_manager->GetAllDownloads(
          FilePath(FILE_PATH_LITERAL("")), &all_items);
  }
  query_out.Search(all_items.begin(), all_items.end(), results);
}

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

  content::DownloadSaveInfo save_info;
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
    save_info.suggested_name = filename16;
  }

  if (options.save_as.get())
    save_info.prompt_for_save_location = *options.save_as.get();

  Profile* current_profile = profile();
  if (include_incognito() && profile()->HasOffTheRecordProfile())
    current_profile = profile()->GetOffTheRecordProfile();

  scoped_ptr<content::DownloadUrlParameters> download_params(
      new content::DownloadUrlParameters(
          download_url,
          render_view_host()->GetProcess()->GetID(),
          render_view_host()->GetRoutingID(),
          current_profile->GetResourceContext(),
          save_info));

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

  if (options.method.get())
    download_params->set_method(*options.method.get());
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

void DownloadsDownloadFunction::OnStarted(DownloadId dl_id, net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << __FUNCTION__ << " " << dl_id << " " << error;
  if (dl_id.local() >= 0) {
    SetResult(base::Value::CreateIntegerValue(dl_id.local()));
  } else {
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
  DownloadQuery::DownloadVector results;
  RunDownloadQuery(params->query, profile(), include_incognito(),
                   &error_, &results);
  if (!error_.empty())
    return false;
  base::ListValue* json_results = new base::ListValue();
  for (DownloadManager::DownloadVector::const_iterator it = results.begin();
       it != results.end(); ++it) {
    scoped_ptr<base::DictionaryValue> item(DownloadItemToJSON(*it));
    json_results->Append(item.release());
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
  DownloadItem* download_item = GetActiveItem(
      profile(), include_incognito(), params->download_id);
  if ((download_item == NULL) || !download_item->IsInProgress()) {
    // This could be due to an invalid download ID, or it could be due to the
    // download not being currently active.
    error_ = download_extension_errors::kInvalidOperationError;
  } else if (!download_item->IsPaused()) {
    // If download_item->IsPaused() already then we treat it as a success.
    download_item->TogglePause();
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
  DownloadItem* download_item = GetActiveItem(
      profile(), include_incognito(), params->download_id);
  if (download_item == NULL) {
    // This could be due to an invalid download ID, or it could be due to the
    // download not being currently active.
    error_ = download_extension_errors::kInvalidOperationError;
  } else if (download_item->IsPaused()) {
    // If !download_item->IsPaused() already, then we treat it as a success.
    download_item->TogglePause();
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
  DownloadItem* download_item = GetActiveItem(
      profile(), include_incognito(), params->download_id);
  if (download_item != NULL)
    download_item->Cancel(true);
  // |download_item| can be NULL if the download ID was invalid or if the
  // download is not currently active.  Either way, we don't consider it a
  // failure.
  if (error_.empty())
    RecordApiFunctions(DOWNLOADS_FUNCTION_CANCEL);
  return error_.empty();
}

DownloadsEraseFunction::DownloadsEraseFunction() {}
DownloadsEraseFunction::~DownloadsEraseFunction() {}

bool DownloadsEraseFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Erase::Params> params(
      extensions::api::downloads::Erase::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  error_ = download_extension_errors::kNotImplementedError;
  if (error_.empty())
    RecordApiFunctions(DOWNLOADS_FUNCTION_ERASE);
  return error_.empty();
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
  error_ = download_extension_errors::kNotImplementedError;
  if (error_.empty())
    RecordApiFunctions(DOWNLOADS_FUNCTION_ACCEPT_DANGER);
  return error_.empty();
}

DownloadsShowFunction::DownloadsShowFunction() {}
DownloadsShowFunction::~DownloadsShowFunction() {}

bool DownloadsShowFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Show::Params> params(
      extensions::api::downloads::Show::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  error_ = download_extension_errors::kNotImplementedError;
  if (error_.empty())
    RecordApiFunctions(DOWNLOADS_FUNCTION_SHOW);
  return error_.empty();
}

DownloadsOpenFunction::DownloadsOpenFunction() {}
DownloadsOpenFunction::~DownloadsOpenFunction() {}

bool DownloadsOpenFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Open::Params> params(
      extensions::api::downloads::Open::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  error_ = download_extension_errors::kNotImplementedError;
  if (error_.empty())
    RecordApiFunctions(DOWNLOADS_FUNCTION_OPEN);
  return error_.empty();
}

DownloadsDragFunction::DownloadsDragFunction() {}
DownloadsDragFunction::~DownloadsDragFunction() {}

bool DownloadsDragFunction::RunImpl() {
  scoped_ptr<extensions::api::downloads::Drag::Params> params(
      extensions::api::downloads::Drag::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  error_ = download_extension_errors::kNotImplementedError;
  if (error_.empty())
    RecordApiFunctions(DOWNLOADS_FUNCTION_DRAG);
  return error_.empty();
}

DownloadsGetFileIconFunction::DownloadsGetFileIconFunction()
  : icon_size_(kDefaultIconSize),
    icon_extractor_(new DownloadFileIconExtractorImpl()) {
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
  DownloadManager* manager = NULL;
  DownloadManager* incognito_manager = NULL;
  GetManagers(profile(), include_incognito(), &manager, &incognito_manager);
  DownloadItem* download_item = manager->GetDownloadItem(params->download_id);
  if (!download_item && incognito_manager)
    download_item = incognito_manager->GetDownloadItem(params->download_id);
  if (!download_item) {
    // The DownloadItem is is added to history when the path is determined. If
    // the download is not in history, then we don't have a path / final
    // filename and no icon.
    error_ = download_extension_errors::kInvalidOperationError;
    return false;
  }
  // In-progress downloads return the intermediate filename for GetFullPath()
  // which doesn't have the final extension. Therefore we won't be able to
  // derive a good file icon for it. So we use GetTargetFilePath() instead.
  FilePath path = download_item->GetTargetFilePath();
  DCHECK(!path.empty());
  DCHECK(icon_extractor_.get());
  DCHECK(icon_size == 16 || icon_size == 32);
  EXTENSION_FUNCTION_VALIDATE(icon_extractor_->ExtractIconURLForPath(
      path, IconLoaderSizeFromPixelSize(icon_size),
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
    manager_(manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  DCHECK(manager_);
  manager_->AddObserver(this);
}

ExtensionDownloadsEventRouter::~ExtensionDownloadsEventRouter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (manager_ != NULL)
    manager_->RemoveObserver(this);
  for (ItemMap::const_iterator iter = downloads_.begin();
       iter != downloads_.end(); ++iter) {
    if (iter->second != NULL)
      iter->second->RemoveObserver(this);
  }
  STLDeleteValues(&item_jsons_);
  STLDeleteValues(&on_changed_stats_);
}

ExtensionDownloadsEventRouter::OnChangedStat::OnChangedStat()
  : fires(0),
    total(0) {
}

ExtensionDownloadsEventRouter::OnChangedStat::~OnChangedStat() {
  if (total > 0)
    UMA_HISTOGRAM_PERCENTAGE("Download.OnChanged", (fires * 100 / total));
}

void ExtensionDownloadsEventRouter::OnDownloadUpdated(DownloadItem* item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int download_id = item->GetId();
  if (item->GetState() == DownloadItem::REMOVING) {
    // The REMOVING state indicates that this item is being erased.
    // Let's unregister as an observer so that we don't see any more updates
    // from it, dispatch the onErased event, and remove its json and is
    // OnChangedStat from our maps.
    downloads_.erase(download_id);
    item->RemoveObserver(this);
    DispatchEvent(extension_event_names::kOnDownloadErased,
                  base::Value::CreateIntegerValue(download_id));
    delete item_jsons_[download_id];
    item_jsons_.erase(download_id);
    delete on_changed_stats_[download_id];
    on_changed_stats_.erase(download_id);
    return;
  }

  base::DictionaryValue* old_json = item_jsons_[download_id];
  scoped_ptr<base::DictionaryValue> new_json(DownloadItemToJSON(item));
  scoped_ptr<base::DictionaryValue> delta(new base::DictionaryValue());
  delta->SetInteger(kIdKey, download_id);
  std::set<std::string> new_fields;
  bool changed = false;

  // For each field in the new json representation of the item except the
  // bytesReceived field, if the field has changed from the previous old json,
  // set the differences in the |delta| object and remember that something
  // significant changed.
  for (base::DictionaryValue::Iterator iter(*new_json.get());
       iter.HasNext(); iter.Advance()) {
    new_fields.insert(iter.key());
    if (iter.key() != kBytesReceivedKey) {
      base::Value* old_value = NULL;
      if (!old_json->HasKey(iter.key()) ||
          (old_json->Get(iter.key(), &old_value) &&
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
  for (base::DictionaryValue::Iterator iter(*old_json);
       iter.HasNext(); iter.Advance()) {
    if (new_fields.find(iter.key()) == new_fields.end()) {
      delta->Set(iter.key() + ".previous", iter.value().DeepCopy());
      changed = true;
    }
  }

  // Update the OnChangedStat and dispatch the event if something significant
  // changed. Replace the stored json with the new json.
  ++(on_changed_stats_[download_id]->total);
  if (changed) {
    DispatchEvent(extension_event_names::kOnDownloadChanged, delta.release());
    ++(on_changed_stats_[download_id]->fires);
  }
  item_jsons_[download_id]->Swap(new_json.get());
}

void ExtensionDownloadsEventRouter::OnDownloadOpened(DownloadItem* item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ExtensionDownloadsEventRouter::ModelChanged(DownloadManager* manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(manager_ == manager);
  typedef std::set<int> DownloadIdSet;

  // Get all the download items.
  DownloadManager::DownloadVector current_vec;
  manager_->SearchDownloads(string16(), &current_vec);

  // Populate set<>s of download item identifiers so that we can find
  // differences between the old and the new set of download items.
  DownloadIdSet current_set, prev_set;
  for (ItemMap::const_iterator iter = downloads_.begin();
       iter != downloads_.end(); ++iter) {
    prev_set.insert(iter->first);
  }
  ItemMap current_map;
  for (DownloadManager::DownloadVector::const_iterator iter =
       current_vec.begin();
       iter != current_vec.end(); ++iter) {
    DownloadItem* item = *iter;
    int item_id = item->GetId();
    CHECK(item_id >= 0);
    DCHECK(current_map.find(item_id) == current_map.end());
    current_set.insert(item_id);
    current_map[item_id] = item;
  }
  DownloadIdSet new_set;  // current_set - prev_set;
  std::set_difference(current_set.begin(), current_set.end(),
                      prev_set.begin(), prev_set.end(),
                      std::insert_iterator<DownloadIdSet>(
                        new_set, new_set.begin()));

  // For each download that was not in the old set of downloads but is in the
  // new set of downloads, fire an onCreated event, register as an Observer of
  // the item, store a json representation of the item so that we can easily
  // find changes in that json representation, and make an OnChangedStat.
  for (DownloadIdSet::const_iterator iter = new_set.begin();
       iter != new_set.end(); ++iter) {
    scoped_ptr<base::DictionaryValue> item(
        DownloadItemToJSON(current_map[*iter]));
    DispatchEvent(extension_event_names::kOnDownloadCreated, item->DeepCopy());
    DCHECK(item_jsons_.find(*iter) == item_jsons_.end());
    on_changed_stats_[*iter] = new OnChangedStat();
    current_map[*iter]->AddObserver(this);
    item_jsons_[*iter] = item.release();
  }
  downloads_.swap(current_map);

  // Dispatching onErased is handled in OnDownloadUpdated when an item
  // transitions to the REMOVING state.
}

void ExtensionDownloadsEventRouter::ManagerGoingDown(
    DownloadManager* manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  manager_->RemoveObserver(this);
  manager_ = NULL;
}

void ExtensionDownloadsEventRouter::DispatchEvent(
    const char* event_name, base::Value* arg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::ListValue args;
  args.Append(arg);
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name,
      json_args,
      profile_,
      GURL(),
      extensions::EventFilteringInfo());

  DownloadsNotificationSource notification_source;
  notification_source.event_name = event_name;
  notification_source.profile = profile_;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_DOWNLOADS_EVENT,
      content::Source<DownloadsNotificationSource>(&notification_source),
      content::Details<std::string>(&json_args));
}
