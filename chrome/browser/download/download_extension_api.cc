// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_extension_api.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
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
#include "chrome/browser/download/download_file_icon_extractor.h"
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
#include "content/browser/download/download_id.h"
#include "content/browser/download/download_state_info.h"
#include "content/browser/download/download_types.h"
#include "content/browser/download/interrupt_reasons.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/render_process_host.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;

namespace download_extension_errors {

// Error messages
const char kGenericError[] = "I'm afraid I can't do that.";
const char kIconNotFoundError[] = "Icon not found.";
const char kInvalidOperationError[] = "Invalid operation.";
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
const char kSizeKey[] = "size";
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
  if (options->HasKey(kFilenameKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetString(
        kFilenameKey, &iodata_->filename));
  // TODO(benjhayden): More robust validation of filename.
  if (((iodata_->filename[0] == L'.') && (iodata_->filename[1] == L'.')) ||
      (iodata_->filename[0] == L'/')) {
    error_ = download_extension_errors::kGenericError;
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
        error_ = download_extension_errors::kGenericError;
        return false;
      }
    }
  }
  iodata_->rdh = ResourceDispatcherHost::Get();
  iodata_->resource_context = &profile()->GetResourceContext();
  iodata_->render_process_host_id = render_view_host()->process()->GetID();
  iodata_->render_view_host_routing_id = render_view_host()->routing_id();
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
  error_ = download_extension_errors::kNotImplementedError;
  return false;
}

bool DownloadsSearchFunction::RunInternal() {
  NOTIMPLEMENTED();
  return false;
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

namespace {

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
    url = web_ui_util::GetImageDataUrl(*icon);
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

} // namespace

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

namespace {
base::DictionaryValue* DownloadItemToJSON(DownloadItem* item) {
  base::DictionaryValue* json = new base::DictionaryValue();
  json->SetInteger(kIdKey, item->GetId());
  json->SetString(kUrlKey, item->GetOriginalUrl().spec());
  json->SetString(kFilenameKey, item->GetFullPath().LossyDisplayName());
  json->SetString(kDangerKey, DangerString(item->GetDangerType()));
  json->SetBoolean(kDangerAcceptedKey,
      item->GetSafetyState() == DownloadItem::DANGEROUS_BUT_VALIDATED);
  json->SetString(kStateKey, StateString(item->GetState()));
  json->SetBoolean(kPausedKey, item->IsPaused());
  json->SetString(kMimeKey, item->GetMimeType());
  json->SetInteger(kStartTimeKey,
      (item->GetStartTime() - base::Time::UnixEpoch()).InMilliseconds());
  json->SetInteger(kBytesReceivedKey, item->GetReceivedBytes());
  json->SetInteger(kTotalBytesKey, item->GetTotalBytes());
  if (item->GetState() == DownloadItem::INTERRUPTED)
    json->SetInteger(kErrorKey, static_cast<int>(item->GetLastReason()));
  // TODO(benjhayden): Implement endTime and fileSize.
  // json->SetInteger(kEndTimeKey, -1);
  json->SetInteger(kFileSizeKey, item->GetTotalBytes());
  return json;
}
}  // anonymous namespace

ExtensionDownloadsEventRouter::ExtensionDownloadsEventRouter(Profile* profile)
  : profile_(profile),
    manager_(NULL) {
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
    int item_id = item->GetId();
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
