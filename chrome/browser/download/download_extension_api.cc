// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_extension_api.h"

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
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

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
const char kHeaderBinaryValueKey[] = "binaryValue";
const char kHeadersKey[] = "headers";
const char kIdKey[] = "id";
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
// corresponding change to {experimental.}downloads.json.
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
// corresponding change to {experimental.}downloads.json.
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
    url = web_ui_util::GetImageDataUrl(*icon->ToSkBitmap());
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

}  // namespace

bool DownloadsFunctionInterface::RunImplImpl(
    DownloadsFunctionInterface* pimpl) {
  CHECK(pimpl);
  if (!pimpl->ParseArgs()) return false;
  UMA_HISTOGRAM_ENUMERATION(
      "Download.ApiFunctions", pimpl->function(), DOWNLOADS_FUNCTION_LAST);
  return pimpl->RunInternal();
}

SyncDownloadsFunction::SyncDownloadsFunction(
    DownloadsFunctionInterface::DownloadsFunctionName function)
  : function_(function) {
}

SyncDownloadsFunction::~SyncDownloadsFunction() {}

bool SyncDownloadsFunction::RunImpl() {
  return DownloadsFunctionInterface::RunImplImpl(this);
}

DownloadsFunctionInterface::DownloadsFunctionName
SyncDownloadsFunction::function() const {
  return function_;
}

AsyncDownloadsFunction::AsyncDownloadsFunction(
    DownloadsFunctionInterface::DownloadsFunctionName function)
  : function_(function) {
}

AsyncDownloadsFunction::~AsyncDownloadsFunction() {}

bool AsyncDownloadsFunction::RunImpl() {
  return DownloadsFunctionInterface::RunImplImpl(this);
}

DownloadsFunctionInterface::DownloadsFunctionName
AsyncDownloadsFunction::function() const {
  return function_;
}

DownloadsDownloadFunction::DownloadsDownloadFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_DOWNLOAD) {
}

DownloadsDownloadFunction::~DownloadsDownloadFunction() {}

DownloadsDownloadFunction::IOData::IOData()
  : save_as(false),
    extra_headers(NULL),
    method("GET"),
    rdh(NULL),
    resource_context(NULL),
    render_process_host_id(0),
    render_view_host_routing_id(0) {
}

DownloadsDownloadFunction::IOData::~IOData() {}

bool DownloadsDownloadFunction::ParseArgs() {
  base::DictionaryValue* options = NULL;
  std::string url;
  iodata_.reset(new IOData());
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options));
  EXTENSION_FUNCTION_VALIDATE(options->GetString(kUrlKey, &url));
  iodata_->url = GURL(url);
  if (!iodata_->url.is_valid()) {
    error_ = download_extension_errors::kInvalidURLError;
    return false;
  }

  if (options->HasKey(kFilenameKey)) {
    EXTENSION_FUNCTION_VALIDATE(options->GetString(
        kFilenameKey, &iodata_->filename));
    if (!ValidateFilename(iodata_->filename)) {
      error_ = download_extension_errors::kGenericError;
      return false;
    }
  }

  if (options->HasKey(kSaveAsKey)) {
    EXTENSION_FUNCTION_VALIDATE(options->GetBoolean(
        kSaveAsKey, &iodata_->save_as));
  }

  if (options->HasKey(kMethodKey)) {
    EXTENSION_FUNCTION_VALIDATE(options->GetString(
        kMethodKey, &iodata_->method));
  }

  // It's ok to use a pointer to extra_headers without DeepCopy()ing because
  // |args_| (which owns *extra_headers) is guaranteed to live as long as
  // |this|.
  if (options->HasKey(kHeadersKey)) {
    EXTENSION_FUNCTION_VALIDATE(options->GetList(
        kHeadersKey, &iodata_->extra_headers));
  }

  if (options->HasKey(kBodyKey)) {
    EXTENSION_FUNCTION_VALIDATE(options->GetString(
        kBodyKey, &iodata_->post_body));
  }

  if (iodata_->extra_headers != NULL) {
    for (size_t index = 0; index < iodata_->extra_headers->GetSize(); ++index) {
      base::DictionaryValue* header = NULL;
      std::string name;
      EXTENSION_FUNCTION_VALIDATE(iodata_->extra_headers->GetDictionary(
            index, &header));
      EXTENSION_FUNCTION_VALIDATE(header->GetString(
            kHeaderNameKey, &name));
      if (header->HasKey(kHeaderBinaryValueKey)) {
        base::ListValue* binary_value = NULL;
        EXTENSION_FUNCTION_VALIDATE(header->GetList(
              kHeaderBinaryValueKey, &binary_value));
        for (size_t char_i = 0; char_i < binary_value->GetSize(); ++char_i) {
          int char_value = 0;
          EXTENSION_FUNCTION_VALIDATE(binary_value->GetInteger(
                char_i, &char_value));
        }
      } else if (header->HasKey(kHeaderValueKey)) {
        std::string value;
        EXTENSION_FUNCTION_VALIDATE(header->GetString(
              kHeaderValueKey, &value));
      }
      if (!net::HttpUtil::IsSafeHeader(name)) {
        error_ = download_extension_errors::kGenericError;
        return false;
      }
    }
  }
  iodata_->rdh = content::ResourceDispatcherHost::Get();
  iodata_->resource_context = profile()->GetResourceContext();
  iodata_->render_process_host_id = render_view_host()->GetProcess()->GetID();
  iodata_->render_view_host_routing_id = render_view_host()->GetRoutingID();
  return true;
}

bool DownloadsDownloadFunction::RunInternal() {
  VLOG(1) << __FUNCTION__ << " " << iodata_->url.spec();
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
          &DownloadsDownloadFunction::BeginDownloadOnIOThread, this))) {
    error_ = download_extension_errors::kGenericError;
    return false;
  }
  return true;
}

void DownloadsDownloadFunction::BeginDownloadOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << iodata_->url.spec();
  content::DownloadSaveInfo save_info;
  // TODO(benjhayden) Ensure that this filename is interpreted as a path
  // relative to the default downloads directory without allowing '..'.
  save_info.suggested_name = iodata_->filename;
  save_info.prompt_for_save_location = iodata_->save_as;

  scoped_ptr<net::URLRequest> request(new net::URLRequest(iodata_->url, NULL));
  request->set_method(iodata_->method);
  if (iodata_->extra_headers != NULL) {
    for (size_t index = 0; index < iodata_->extra_headers->GetSize(); ++index) {
      base::DictionaryValue* header = NULL;
      std::string name, value;
      CHECK(iodata_->extra_headers->GetDictionary(index, &header));
      CHECK(header->GetString(kHeaderNameKey, &name));
      if (header->HasKey(kHeaderBinaryValueKey)) {
        base::ListValue* binary_value = NULL;
        CHECK(header->GetList(kHeaderBinaryValueKey, &binary_value));
        for (size_t char_i = 0; char_i < binary_value->GetSize(); ++char_i) {
          int char_value = 0;
          CHECK(binary_value->GetInteger(char_i, &char_value));
          if ((0 <= char_value) &&
              (char_value <= 0xff)) {
            value.push_back(char_value);
          }
        }
      } else if (header->HasKey(kHeaderValueKey)) {
        CHECK(header->GetString(kHeaderValueKey, &value));
      }
      request->SetExtraRequestHeaderByName(name, value, false/*overwrite*/);
    }
  }
  if (!iodata_->post_body.empty()) {
    request->AppendBytesToUpload(iodata_->post_body.data(),
                                 iodata_->post_body.size());
  }

  // Prevent login prompts for 401/407 responses.
  request->set_load_flags(request->load_flags() |
                          net::LOAD_DO_NOT_PROMPT_FOR_LOGIN);

  net::Error error = iodata_->rdh->BeginDownload(
      request.Pass(),
      false,  // is_content_initiated
      iodata_->resource_context,
      iodata_->render_process_host_id,
      iodata_->render_view_host_routing_id,
      false,  // prefer_cache
      save_info,
      base::Bind(&DownloadsDownloadFunction::OnStarted, this));
  iodata_.reset();

  if (error != net::OK) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DownloadsDownloadFunction::OnStarted, this,
                   DownloadId::Invalid(), error));
  }
}

void DownloadsDownloadFunction::OnStarted(DownloadId dl_id, net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << __FUNCTION__ << " " << dl_id << " " << error;
  if (dl_id.local() >= 0) {
    result_.reset(base::Value::CreateIntegerValue(dl_id.local()));
  } else {
    error_ = net::ErrorToString(error);
  }
  SendResponse(error_.empty());
}

DownloadsSearchFunction::DownloadsSearchFunction()
  : SyncDownloadsFunction(DOWNLOADS_FUNCTION_SEARCH),
    query_(new DownloadQuery()),
    get_id_(0),
    has_get_id_(false) {
}

DownloadsSearchFunction::~DownloadsSearchFunction() {}

bool DownloadsSearchFunction::ParseArgs() {
  static base::LazyInstance<FilterTypeMap> filter_types =
    LAZY_INSTANCE_INITIALIZER;
  if (filter_types.Get().size() == 0)
    InitFilterTypeMap(filter_types.Get());

  base::DictionaryValue* query_json = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &query_json));
  for (base::DictionaryValue::Iterator query_json_field(*query_json);
       query_json_field.HasNext(); query_json_field.Advance()) {
    FilterTypeMap::const_iterator filter_type =
        filter_types.Get().find(query_json_field.key());

    if (filter_type != filter_types.Get().end()) {
      if (!query_->AddFilter(filter_type->second, query_json_field.value())) {
        error_ = download_extension_errors::kInvalidFilterError;
        return false;
      }
    } else if (query_json_field.key() == kIdKey) {
      EXTENSION_FUNCTION_VALIDATE(query_json_field.value().GetAsInteger(
          &get_id_));
      has_get_id_ = true;
    } else if (query_json_field.key() == kOrderByKey) {
      if (!ParseOrderBy(query_json_field.value()))
        return false;
    } else if (query_json_field.key() == kDangerKey) {
      std::string danger_str;
      EXTENSION_FUNCTION_VALIDATE(query_json_field.value().GetAsString(
          &danger_str));
      content::DownloadDangerType danger_type =
          DangerEnumFromString(danger_str);
      if (danger_type == content::DOWNLOAD_DANGER_TYPE_MAX) {
        error_ = download_extension_errors::kInvalidDangerTypeError;
        return false;
      }
      query_->AddFilter(danger_type);
    } else if (query_json_field.key() == kStateKey) {
      std::string state_str;
      EXTENSION_FUNCTION_VALIDATE(query_json_field.value().GetAsString(
          &state_str));
      DownloadItem::DownloadState state = StateEnumFromString(state_str);
      if (state == DownloadItem::MAX_DOWNLOAD_STATE) {
        error_ = download_extension_errors::kInvalidStateError;
        return false;
      }
      query_->AddFilter(state);
    } else if (query_json_field.key() == kLimitKey) {
      int limit = 0;
      EXTENSION_FUNCTION_VALIDATE(query_json_field.value().GetAsInteger(
          &limit));
      if (limit < 0) {
        error_ = download_extension_errors::kInvalidQueryLimit;
        return false;
      }
      query_->Limit(limit);
    } else {
      EXTENSION_FUNCTION_VALIDATE(false);
    }
  }
  return true;
}

bool DownloadsSearchFunction::ParseOrderBy(const base::Value& order_by_value) {
  static base::LazyInstance<SortTypeMap> sorter_types =
    LAZY_INSTANCE_INITIALIZER;
  if (sorter_types.Get().size() == 0)
    InitSortTypeMap(sorter_types.Get());

  std::string order_by_str;
  EXTENSION_FUNCTION_VALIDATE(order_by_value.GetAsString(&order_by_str));
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
      error_ = download_extension_errors::kInvalidOrderByError;
      return false;
    }
    query_->AddSorter(sorter_type->second, direction);
  }
  return true;
}

bool DownloadsSearchFunction::RunInternal() {
  DownloadQuery::DownloadVector all_items, cpp_results;
  DownloadManager* manager = DownloadServiceFactory::GetForProfile(profile())
    ->GetDownloadManager();
  if (has_get_id_) {
    DownloadItem* item = manager->GetDownloadItem(get_id_);
    if (item != NULL)
      all_items.push_back(item);
  } else {
    manager->GetAllDownloads(FilePath(FILE_PATH_LITERAL("")), &all_items);
  }
  query_->Search(all_items.begin(), all_items.end(), &cpp_results);
  base::ListValue* json_results = new base::ListValue();
  for (DownloadManager::DownloadVector::const_iterator it = cpp_results.begin();
       it != cpp_results.end(); ++it) {
    scoped_ptr<base::DictionaryValue> item(DownloadItemToJSON(*it));
    json_results->Append(item.release());
  }
  result_.reset(json_results);
  return true;
}

DownloadsPauseFunction::DownloadsPauseFunction()
  : SyncDownloadsFunction(DOWNLOADS_FUNCTION_PAUSE),
    download_id_(DownloadId::Invalid().local()) {
}

DownloadsPauseFunction::~DownloadsPauseFunction() {}

bool DownloadsPauseFunction::ParseArgs() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &download_id_));
  return true;
}

bool DownloadsPauseFunction::RunInternal() {
  DownloadManager* download_manager =
      DownloadServiceFactory::GetForProfile(profile())->GetDownloadManager();
  DownloadItem* download_item =
      download_manager->GetActiveDownloadItem(download_id_);
  DCHECK(!download_item || download_item->IsInProgress());

  if (!download_item) {
    // This could be due to an invalid download ID, or it could be due to the
    // download not being currently active.
    error_ = download_extension_errors::kInvalidOperationError;
  } else if (!download_item->IsPaused()) {
    // If download_item->IsPaused() already then we treat it as a success.
    download_item->TogglePause();
  }
  return error_.empty();
}

DownloadsResumeFunction::DownloadsResumeFunction()
  : SyncDownloadsFunction(DOWNLOADS_FUNCTION_RESUME),
    download_id_(DownloadId::Invalid().local()) {
}

DownloadsResumeFunction::~DownloadsResumeFunction() {}

bool DownloadsResumeFunction::ParseArgs() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &download_id_));
  return true;
}

bool DownloadsResumeFunction::RunInternal() {
  DownloadManager* download_manager =
      DownloadServiceFactory::GetForProfile(profile())->GetDownloadManager();
  DownloadItem* download_item =
      download_manager->GetActiveDownloadItem(download_id_);
  DCHECK(!download_item || download_item->IsInProgress());

  if (!download_item) {
    // This could be due to an invalid download ID, or it could be due to the
    // download not being currently active.
    error_ = download_extension_errors::kInvalidOperationError;
  } else if (download_item->IsPaused()) {
    // If !download_item->IsPaused() already, then we treat it as a success.
    download_item->TogglePause();
  }
  return error_.empty();
}

DownloadsCancelFunction::DownloadsCancelFunction()
  : SyncDownloadsFunction(DOWNLOADS_FUNCTION_CANCEL),
    download_id_(DownloadId::Invalid().local()) {
}

DownloadsCancelFunction::~DownloadsCancelFunction() {}

bool DownloadsCancelFunction::ParseArgs() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &download_id_));
  return true;
}

bool DownloadsCancelFunction::RunInternal() {
  DownloadManager* download_manager =
      DownloadServiceFactory::GetForProfile(profile())->GetDownloadManager();
  DownloadItem* download_item =
      download_manager->GetActiveDownloadItem(download_id_);

  if (download_item)
    download_item->Cancel(true);
  // |download_item| can be NULL if the download ID was invalid or if the
  // download is not currently active.  Either way, we don't consider it a
  // failure.
  return error_.empty();
}

DownloadsEraseFunction::DownloadsEraseFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_ERASE) {
}

DownloadsEraseFunction::~DownloadsEraseFunction() {}

bool DownloadsEraseFunction::ParseArgs() {
  base::DictionaryValue* query_json = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &query_json));
  error_ = download_extension_errors::kNotImplementedError;
  return false;
}

bool DownloadsEraseFunction::RunInternal() {
  NOTIMPLEMENTED();
  return false;
}

DownloadsSetDestinationFunction::DownloadsSetDestinationFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_SET_DESTINATION) {
}

DownloadsSetDestinationFunction::~DownloadsSetDestinationFunction() {}

bool DownloadsSetDestinationFunction::ParseArgs() {
  int dl_id = 0;
  std::string path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &path));
  VLOG(1) << __FUNCTION__ << " " << dl_id << " " << &path;
  error_ = download_extension_errors::kNotImplementedError;
  return false;
}

bool DownloadsSetDestinationFunction::RunInternal() {
  NOTIMPLEMENTED();
  return false;
}

DownloadsAcceptDangerFunction::DownloadsAcceptDangerFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_ACCEPT_DANGER) {
}

DownloadsAcceptDangerFunction::~DownloadsAcceptDangerFunction() {}

bool DownloadsAcceptDangerFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = download_extension_errors::kNotImplementedError;
  return false;
}

bool DownloadsAcceptDangerFunction::RunInternal() {
  NOTIMPLEMENTED();
  return false;
}

DownloadsShowFunction::DownloadsShowFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_SHOW) {
}

DownloadsShowFunction::~DownloadsShowFunction() {}

bool DownloadsShowFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = download_extension_errors::kNotImplementedError;
  return false;
}

bool DownloadsShowFunction::RunInternal() {
  NOTIMPLEMENTED();
  return false;
}

DownloadsDragFunction::DownloadsDragFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_DRAG) {
}

DownloadsDragFunction::~DownloadsDragFunction() {}

bool DownloadsDragFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = download_extension_errors::kNotImplementedError;
  return false;
}

bool DownloadsDragFunction::RunInternal() {
  NOTIMPLEMENTED();
  return false;
}

DownloadsGetFileIconFunction::DownloadsGetFileIconFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_GET_FILE_ICON),
    icon_size_(kDefaultIconSize),
    icon_extractor_(new DownloadFileIconExtractorImpl()) {
}

DownloadsGetFileIconFunction::~DownloadsGetFileIconFunction() {}

void DownloadsGetFileIconFunction::SetIconExtractorForTesting(
    DownloadFileIconExtractor* extractor) {
  DCHECK(extractor);
  icon_extractor_.reset(extractor);
}

bool DownloadsGetFileIconFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));

  base::DictionaryValue* options = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &options));
  if (options->HasKey(kSizeKey)) {
    EXTENSION_FUNCTION_VALIDATE(options->GetInteger(kSizeKey, &icon_size_));
    // We only support 16px and 32px icons. This is enforced in
    // experimental.downloads.json.
    DCHECK(icon_size_ == 16 || icon_size_ == 32);
  }

  DownloadManager* download_manager =
      DownloadServiceFactory::GetForProfile(profile())->GetDownloadManager();
  DownloadItem* download_item = download_manager->GetDownloadItem(dl_id);
  if (download_item == NULL) {
    // The DownloadItem is is added to history when the path is determined. If
    // the download is not in history, then we don't have a path / final
    // filename and no icon.
    error_ = download_extension_errors::kInvalidOperationError;
    return false;
  }
  // In-progress downloads return the intermediate filename for GetFullPath()
  // which doesn't have the final extension. Therefore we won't be able to
  // derive a good file icon for it. So we use GetTargetFilePath() instead.
  path_ = download_item->GetTargetFilePath();
  DCHECK(!path_.empty());
  return true;
}

bool DownloadsGetFileIconFunction::RunInternal() {
  DCHECK(!path_.empty());
  DCHECK(icon_extractor_.get());
  EXTENSION_FUNCTION_VALIDATE(icon_extractor_->ExtractIconURLForPath(
      path_, IconLoaderSizeFromPixelSize(icon_size_),
      base::Bind(&DownloadsGetFileIconFunction::OnIconURLExtracted, this)));
  return true;
}

void DownloadsGetFileIconFunction::OnIconURLExtracted(const std::string& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (url.empty())
    error_ = download_extension_errors::kIconNotFoundError;
  else
    result_.reset(base::Value::CreateStringValue(url));
  SendResponse(error_.empty());
}

ExtensionDownloadsEventRouter::ExtensionDownloadsEventRouter(Profile* profile)
  : profile_(profile),
    manager_(NULL),
    delete_item_jsons_(&item_jsons_),
    delete_on_changed_stats_(&on_changed_stats_) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  // Register a callback with the DownloadService for this profile to be called
  // when it creates the DownloadManager, or now if the manager already exists.
  DownloadServiceFactory::GetForProfile(profile)->OnManagerCreated(base::Bind(
      &ExtensionDownloadsEventRouter::Init, base::Unretained(this)));
}

// The only public methods on this class are ModelChanged() and
// ManagerGoingDown(), and they are only called by DownloadManager, so
// there's no way for any methods on this class to be called before
// DownloadService calls Init() via the OnManagerCreated Callback above.
void ExtensionDownloadsEventRouter::Init(DownloadManager* manager) {
  DCHECK(manager_ == NULL);
  manager_ = manager;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  manager_->AddObserver(this);
}

ExtensionDownloadsEventRouter::~ExtensionDownloadsEventRouter() {
  if (manager_ != NULL)
    manager_->RemoveObserver(this);
  for (ItemMap::const_iterator iter = downloads_.begin();
       iter != downloads_.end(); ++iter) {
    if (iter->second != NULL)
      iter->second->RemoveObserver(this);
  }
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
        delta->Set(iter.key() + ".new", iter.value().DeepCopy());
        if (old_value)
          delta->Set(iter.key() + ".old", old_value->DeepCopy());
        changed = true;
      }
    }
  }

  // If a field was in the previous json but is not in the new json, set the
  // difference in |delta|.
  for (base::DictionaryValue::Iterator iter(*old_json);
       iter.HasNext(); iter.Advance()) {
    if (new_fields.find(iter.key()) == new_fields.end()) {
      delta->Set(iter.key() + ".old", iter.value().DeepCopy());
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
  manager_->RemoveObserver(this);
  manager_ = NULL;
}

void ExtensionDownloadsEventRouter::DispatchEvent(
    const char* event_name, base::Value* arg) {
  ListValue args;
  args.Append(arg);
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name,
      json_args,
      profile_,
      GURL());
}
