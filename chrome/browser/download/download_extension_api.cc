// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_extension_api.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_state_info.h"
#include "content/browser/download/download_types.h"
#include "content/browser/download/interrupt_reasons.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

namespace {

// Error messages
const char kNotImplemented[] = "NotImplemented";
const char kGenericError[] = "I'm afraid I can't do that.";
const char kInvalidURL[] = "Invalid URL";

// Parameter keys
const char kBodyKey[] = "body";
const char kBytesReceivedKey[] = "bytesReceived";
const char kDangerAcceptedKey[] = "dangerAccepted";
const char kDangerContent[] = "content";
const char kDangerFile[] = "file";
const char kDangerKey[] = "danger";
const char kDangerSafe[] = "safe";
const char kDangerUrl[] = "url";
const char kEndTimeKey[] = "endTime";
const char kErrorKey[] = "error";
const char kFileSizeKey[] = "fileSize";
const char kFilenameKey[] = "filename";
const char kHeaderNameKey[] = "name";
const char kHeaderValueKey[] = "value";
const char kHeadersKey[] = "headers";
const char kIdKey[] = "id";
const char kMethodKey[] = "method";
const char kMimeKey[] = "mime";
const char kPausedKey[] = "paused";
const char kSaveAsKey[] = "saveAs";
const char kStartTimeKey[] = "startTime";
const char kStateComplete[] = "complete";
const char kStateInProgress[] = "in_progress";
const char kStateInterrupted[] = "interrupted";
const char kStateKey[] = "state";
const char kTotalBytesKey[] = "totalBytes";
const char kUrlKey[] = "url";

const char* DangerString(DownloadStateInfo::DangerType danger) {
  switch (danger) {
    case DownloadStateInfo::MAYBE_DANGEROUS_CONTENT:
    case DownloadStateInfo::NOT_DANGEROUS: return kDangerSafe;
    case DownloadStateInfo::DANGEROUS_FILE: return kDangerFile;
    case DownloadStateInfo::DANGEROUS_URL: return kDangerUrl;
    case DownloadStateInfo::DANGEROUS_CONTENT: return kDangerContent;
    default:
      NOTREACHED();
      return "";
  }
}

const char* StateString(DownloadItem::DownloadState state) {
  switch (state) {
    case DownloadItem::IN_PROGRESS: return kStateInProgress;
    case DownloadItem::COMPLETE: return kStateComplete;
    case DownloadItem::INTERRUPTED:  // fall through
    case DownloadItem::CANCELLED: return kStateInterrupted;
    case DownloadItem::REMOVING:  // fall through
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

bool DownloadsFunctionInterface::RunImplImpl(
    DownloadsFunctionInterface* pimpl) {
  CHECK(pimpl);
  if (!pimpl->ParseArgs()) return false;
  UMA_HISTOGRAM_ENUMERATION(
      "Download.ApiFunctions", pimpl->function(), DOWNLOADS_FUNCTION_LAST);
  pimpl->RunInternal();
  return true;
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
    error_ = kInvalidURL;
    return false;
  }
  if (options->HasKey(kFilenameKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetString(
        kFilenameKey, &iodata_->filename));
  // TODO(benjhayden): More robust validation of filename.
  if (((iodata_->filename[0] == L'.') && (iodata_->filename[1] == L'.')) ||
      (iodata_->filename[0] == L'/')) {
    error_ = kGenericError;
    return false;
  }
  if (options->HasKey(kSaveAsKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetBoolean(
        kSaveAsKey, &iodata_->save_as));
  if (options->HasKey(kMethodKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetString(
        kMethodKey, &iodata_->method));
  // It's ok to use a pointer to extra_headers without DeepCopy()ing because
  // |args_| (which owns *extra_headers) is guaranteed to live as long as
  // |this|.
  if (options->HasKey(kHeadersKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetList(
        kHeadersKey, &iodata_->extra_headers));
  if (options->HasKey(kBodyKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetString(
        kBodyKey, &iodata_->post_body));
  if (iodata_->extra_headers != NULL) {
    for (size_t index = 0; index < iodata_->extra_headers->GetSize(); ++index) {
      base::DictionaryValue* header = NULL;
      std::string name, value;
      EXTENSION_FUNCTION_VALIDATE(iodata_->extra_headers->GetDictionary(
            index, &header));
      EXTENSION_FUNCTION_VALIDATE(header->GetString(
            kHeaderNameKey, &name));
      EXTENSION_FUNCTION_VALIDATE(header->GetString(
            kHeaderValueKey, &value));
      if (!net::HttpUtil::IsSafeHeader(name)) {
        error_ = kGenericError;
        return false;
      }
    }
  }
  iodata_->rdh = g_browser_process->resource_dispatcher_host();
  iodata_->resource_context = &profile()->GetResourceContext();
  iodata_->render_process_host_id = render_view_host()->process()->id();
  iodata_->render_view_host_routing_id = render_view_host()->routing_id();
  return true;
}

void DownloadsDownloadFunction::RunInternal() {
  VLOG(1) << __FUNCTION__ << " " << iodata_->url.spec();
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
          &DownloadsDownloadFunction::BeginDownloadOnIOThread, this))) {
    error_ = kGenericError;
    SendResponse(error_.empty());
  }
}

void DownloadsDownloadFunction::BeginDownloadOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << iodata_->url.spec();
  DownloadSaveInfo save_info;
  // TODO(benjhayden) Ensure that this filename is interpreted as a path
  // relative to the default downloads directory without allowing '..'.
  save_info.suggested_name = iodata_->filename;
  net::URLRequest* request = new net::URLRequest(iodata_->url, iodata_->rdh);
  request->set_method(iodata_->method);
  if (iodata_->extra_headers != NULL) {
    for (size_t index = 0; index < iodata_->extra_headers->GetSize(); ++index) {
      base::DictionaryValue* header = NULL;
      std::string name, value;
      CHECK(iodata_->extra_headers->GetDictionary(index, &header));
      CHECK(header->GetString("name", &name));
      CHECK(header->GetString("value", &value));
      request->SetExtraRequestHeaderByName(name, value, false/*overwrite*/);
    }
  }
  if (!iodata_->post_body.empty()) {
    request->AppendBytesToUpload(iodata_->post_body.data(),
                                 iodata_->post_body.size());
  }
  iodata_->rdh->BeginDownload(
      request,
      save_info,
      iodata_->save_as,
      base::Bind(&DownloadsDownloadFunction::OnStarted, this),
      iodata_->render_process_host_id,
      iodata_->render_view_host_routing_id,
      *(iodata_->resource_context));
  iodata_.reset();
}

void DownloadsDownloadFunction::OnStarted(DownloadId dl_id, net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << __FUNCTION__ << " " << dl_id << " " << error;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &DownloadsDownloadFunction::RespondOnUIThread, this,
      dl_id.local(), error));
}

void DownloadsDownloadFunction::RespondOnUIThread(int dl_id, net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << __FUNCTION__;
  if (dl_id >= 0) {
    result_.reset(base::Value::CreateIntegerValue(dl_id));
  } else {
    error_ = net::ErrorToString(error);
  }
  SendResponse(error_.empty());
}

DownloadsSearchFunction::DownloadsSearchFunction()
  : SyncDownloadsFunction(DOWNLOADS_FUNCTION_SEARCH) {
}

DownloadsSearchFunction::~DownloadsSearchFunction() {}

bool DownloadsSearchFunction::ParseArgs() {
  base::DictionaryValue* query_json = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &query_json));
  error_ = kNotImplemented;
  return false;
}

void DownloadsSearchFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsPauseFunction::DownloadsPauseFunction()
  : SyncDownloadsFunction(DOWNLOADS_FUNCTION_PAUSE) {
}

DownloadsPauseFunction::~DownloadsPauseFunction() {}

bool DownloadsPauseFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = kNotImplemented;
  return false;
}

void DownloadsPauseFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsResumeFunction::DownloadsResumeFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_RESUME) {
}

DownloadsResumeFunction::~DownloadsResumeFunction() {}

bool DownloadsResumeFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = kNotImplemented;
  return false;
}

void DownloadsResumeFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsCancelFunction::DownloadsCancelFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_CANCEL) {
}

DownloadsCancelFunction::~DownloadsCancelFunction() {}

bool DownloadsCancelFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = kNotImplemented;
  return false;
}

void DownloadsCancelFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsEraseFunction::DownloadsEraseFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_ERASE) {
}

DownloadsEraseFunction::~DownloadsEraseFunction() {}

bool DownloadsEraseFunction::ParseArgs() {
  base::DictionaryValue* query_json = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &query_json));
  error_ = kNotImplemented;
  return false;
}

void DownloadsEraseFunction::RunInternal() {
  NOTIMPLEMENTED();
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
  error_ = kNotImplemented;
  return false;
}

void DownloadsSetDestinationFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsAcceptDangerFunction::DownloadsAcceptDangerFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_ACCEPT_DANGER) {
}

DownloadsAcceptDangerFunction::~DownloadsAcceptDangerFunction() {}

bool DownloadsAcceptDangerFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = kNotImplemented;
  return false;
}

void DownloadsAcceptDangerFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsShowFunction::DownloadsShowFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_SHOW) {
}

DownloadsShowFunction::~DownloadsShowFunction() {}

bool DownloadsShowFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = kNotImplemented;
  return false;
}

void DownloadsShowFunction::RunInternal() {
  NOTIMPLEMENTED();
}

DownloadsDragFunction::DownloadsDragFunction()
  : AsyncDownloadsFunction(DOWNLOADS_FUNCTION_DRAG) {
}

DownloadsDragFunction::~DownloadsDragFunction() {}

bool DownloadsDragFunction::ParseArgs() {
  int dl_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &dl_id));
  VLOG(1) << __FUNCTION__ << " " << dl_id;
  error_ = kNotImplemented;
  return false;
}

void DownloadsDragFunction::RunInternal() {
  NOTIMPLEMENTED();
}

namespace {
base::DictionaryValue* DownloadItemToJSON(DownloadItem* item) {
  base::DictionaryValue* json = new base::DictionaryValue();
  json->SetInteger(kIdKey, item->id());
  json->SetString(kUrlKey, item->original_url().spec());
  json->SetString(kFilenameKey, item->full_path().LossyDisplayName());
  json->SetString(kDangerKey, DangerString(item->GetDangerType()));
  json->SetBoolean(kDangerAcceptedKey,
      item->safety_state() == DownloadItem::DANGEROUS_BUT_VALIDATED);
  json->SetString(kStateKey, StateString(item->state()));
  json->SetBoolean(kPausedKey, item->is_paused());
  json->SetString(kMimeKey, item->mime_type());
  json->SetInteger(kStartTimeKey,
      (item->start_time() - base::Time::UnixEpoch()).InMilliseconds());
  json->SetInteger(kBytesReceivedKey, item->received_bytes());
  json->SetInteger(kTotalBytesKey, item->total_bytes());
  if (item->state() == DownloadItem::INTERRUPTED)
    json->SetInteger(kErrorKey, static_cast<int>(item->last_reason()));
  // TODO(benjhayden): Implement endTime and fileSize.
  // json->SetInteger(kEndTimeKey, -1);
  json->SetInteger(kFileSizeKey, item->total_bytes());
  return json;
}
}  // anonymous namespace

ExtensionDownloadsEventRouter::ExtensionDownloadsEventRouter(
    Profile* profile)
  : profile_(profile),
    manager_(
        profile ?
        DownloadServiceFactory::GetForProfile(profile)->GetDownloadManager() :
        NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  DCHECK(manager_);
  manager_->AddObserver(this);
}

ExtensionDownloadsEventRouter::~ExtensionDownloadsEventRouter() {
  if (manager_ != NULL)
    manager_->RemoveObserver(this);
}

void ExtensionDownloadsEventRouter::ModelChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (manager_ == NULL)
    return;
  DownloadManager::DownloadVector current_vec;
  manager_->SearchDownloads(string16(), &current_vec);
  DownloadIdSet current_set;
  ItemMap current_map;
  for (DownloadManager::DownloadVector::const_iterator iter =
       current_vec.begin();
       iter != current_vec.end(); ++iter) {
    DownloadItem* item = *iter;
    int item_id = item->id();
    // TODO(benjhayden): Remove the following line when every item's id >= 0,
    // which will allow firing onErased events for items from the history.
    if (item_id < 0) continue;
    DCHECK(current_map.find(item_id) == current_map.end());
    current_set.insert(item_id);
    current_map[item_id] = item;
  }
  DownloadIdSet new_set;  // current_set - downloads_;
  DownloadIdSet erased_set;  // downloads_ - current_set;
  std::insert_iterator<DownloadIdSet> new_insertor(new_set, new_set.begin());
  std::insert_iterator<DownloadIdSet> erased_insertor(
      erased_set, erased_set.begin());
  std::set_difference(current_set.begin(), current_set.end(),
                      downloads_.begin(), downloads_.end(),
                      new_insertor);
  std::set_difference(downloads_.begin(), downloads_.end(),
                      current_set.begin(), current_set.end(),
                      erased_insertor);
  for (DownloadIdSet::const_iterator iter = new_set.begin();
       iter != new_set.end(); ++iter) {
    DispatchEvent(extension_event_names::kOnDownloadCreated,
                  DownloadItemToJSON(current_map[*iter]));
  }
  for (DownloadIdSet::const_iterator iter = erased_set.begin();
       iter != erased_set.end(); ++iter) {
    DispatchEvent(extension_event_names::kOnDownloadErased,
                  base::Value::CreateIntegerValue(*iter));
  }
  downloads_.swap(current_set);
}

void ExtensionDownloadsEventRouter::ManagerGoingDown() {
  manager_->RemoveObserver(this);
  manager_ = NULL;
}

void ExtensionDownloadsEventRouter::DispatchEvent(
    const char* event_name, base::Value* arg) {
  ListValue args;
  args.Append(arg);
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name,
      json_args,
      profile_,
      GURL());
}
