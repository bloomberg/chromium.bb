// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/safe_browsing/csd.pb.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

namespace safe_browsing {

const char ClientSideDetectionService::kClientReportPhishingUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/phishing";
const char ClientSideDetectionService::kClientModelUrl[] =
    "https://ssl.gstatic.com/safebrowsing/csd/client_model_v0.pb";

struct ClientSideDetectionService::ClientReportInfo {
  scoped_ptr<ClientReportPhishingRequestCallback> callback;
  GURL phishing_url;
};

ClientSideDetectionService::ClientSideDetectionService(
    const FilePath& model_path,
    URLRequestContextGetter* request_context_getter)
    : model_path_(model_path),
      model_status_(UNKNOWN_STATUS),
      model_file_(base::kInvalidPlatformFileValue),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)),
      request_context_getter_(request_context_getter) {
}

ClientSideDetectionService::~ClientSideDetectionService() {
  method_factory_.RevokeAll();
  STLDeleteContainerPairPointers(client_phishing_reports_.begin(),
                                 client_phishing_reports_.end());
  client_phishing_reports_.clear();
  STLDeleteElements(&open_callbacks_);
  CloseModelFile();
}

/* static */
ClientSideDetectionService* ClientSideDetectionService::Create(
    const FilePath& model_path,
    URLRequestContextGetter* request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ClientSideDetectionService> service(
      new ClientSideDetectionService(model_path, request_context_getter));
  // We try to open the model file right away and start fetching it if
  // it does not already exist on disk.
  base::FileUtilProxy::CreateOrOpenCallback* cb =
      service.get()->callback_factory_.NewCallback(
          &ClientSideDetectionService::OpenModelFileDone);
  if (!base::FileUtilProxy::CreateOrOpen(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          model_path,
          base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
          cb)) {
    delete cb;
    return NULL;
  }
  return service.release();
}

void ClientSideDetectionService::GetModelFile(OpenModelDoneCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &ClientSideDetectionService::StartGetModelFile, callback));
}

void ClientSideDetectionService::SendClientReportPhishingRequest(
    const GURL& phishing_url,
    double score,
    ClientReportPhishingRequestCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &ClientSideDetectionService::StartClientReportPhishingRequest,
          phishing_url, score, callback));
}

void ClientSideDetectionService::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  if (source == model_fetcher_.get()) {
    HandleModelResponse(source, url, status, response_code, cookies, data);
  } else if (client_phishing_reports_.find(source) !=
             client_phishing_reports_.end()) {
    HandlePhishingVerdict(source, url, status, response_code, cookies, data);
  } else {
    NOTREACHED();
  }
}

void ClientSideDetectionService::SetModelStatus(ModelStatus status) {
  DCHECK_NE(READY_STATUS, model_status_);
  model_status_ = status;
  if (READY_STATUS == status || ERROR_STATUS == status) {
    for (size_t i = 0; i < open_callbacks_.size(); ++i) {
      open_callbacks_[i]->Run(model_file_);
    }
    STLDeleteElements(&open_callbacks_);
  } else {
    NOTREACHED();
  }
}

void ClientSideDetectionService::OpenModelFileDone(
    base::PlatformFileError error_code,
    base::PassPlatformFile file,
    bool created) {
  DCHECK(!created);
  if (base::PLATFORM_FILE_OK == error_code) {
    // The model file already exists.  There is no need to fetch the model.
    model_file_ = file.ReleaseValue();
    SetModelStatus(READY_STATUS);
  } else if (base::PLATFORM_FILE_ERROR_NOT_FOUND == error_code) {
    // We need to fetch the model since it does not exist yet.
    model_fetcher_.reset(URLFetcher::Create(0 /* ID is not used */,
                                            GURL(kClientModelUrl),
                                            URLFetcher::GET,
                                            this));
    model_fetcher_->set_request_context(request_context_getter_.get());
    model_fetcher_->Start();
  } else {
    // It is not clear what we should do in this case.  For now we simply fail.
    // Hopefully, we'll be able to read the model during the next browser
    // restart.
    SetModelStatus(ERROR_STATUS);
  }
}

void ClientSideDetectionService::CreateModelFileDone(
    base::PlatformFileError error_code,
    base::PassPlatformFile file,
    bool created) {
  model_file_ = file.ReleaseValue();
  base::FileUtilProxy::ReadWriteCallback* cb = callback_factory_.NewCallback(
      &ClientSideDetectionService::WriteModelFileDone);
  if (!created ||
      base::PLATFORM_FILE_OK != error_code ||
      !base::FileUtilProxy::Write(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          model_file_,
          0 /* offset */, tmp_model_string_->data(), tmp_model_string_->size(),
          cb)) {
    delete cb;
    // An error occurred somewhere.  We close the model file if necessary and
    // then run all the pending callbacks giving them an invalid model file.
    CloseModelFile();
    SetModelStatus(ERROR_STATUS);
  }
}

void ClientSideDetectionService::WriteModelFileDone(
    base::PlatformFileError error_code,
    int bytes_written) {
  if (base::PLATFORM_FILE_OK == error_code) {
    SetModelStatus(READY_STATUS);
  } else {
    // TODO(noelutz): maybe we should retry writing the model since we
    // did already fetch the model?
    CloseModelFile();
    SetModelStatus(ERROR_STATUS);
  }
  // Delete the model string that we kept around while we were writing the
  // string to disk - we don't need it anymore.
  tmp_model_string_.reset();
}

void ClientSideDetectionService::CloseModelFile() {
  if (model_file_ != base::kInvalidPlatformFileValue) {
    base::FileUtilProxy::Close(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
        model_file_,
        NULL);
  }
  model_file_ = base::kInvalidPlatformFileValue;
}

void ClientSideDetectionService::StartGetModelFile(
    OpenModelDoneCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (UNKNOWN_STATUS == model_status_) {
    // Store the callback which will be called once we know the status of the
    // model file.
    open_callbacks_.push_back(callback);
  } else {
    // The model is either in READY or ERROR state which means we can
    // call the callback right away.
    callback->Run(model_file_);
    delete callback;
  }
}

void ClientSideDetectionService::StartClientReportPhishingRequest(
    const GURL& phishing_url,
    double score,
    ClientReportPhishingRequestCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ClientReportPhishingRequestCallback> cb(callback);

  ClientPhishingRequest request;
  request.set_url(phishing_url.spec());
  request.set_client_score(static_cast<float>(score));
  std::string request_data;
  if (!request.SerializeToString(&request_data)) {
    // For consistency, we always call the callback asynchronously, rather than
    // directly from this method.
    LOG(ERROR) << "Unable to serialize the CSD request. Proto file changed?";
    cb->Run(phishing_url, false);
    return;
  }

  URLFetcher* fetcher = URLFetcher::Create(0 /* ID is not used */,
                                           GURL(kClientReportPhishingUrl),
                                           URLFetcher::POST,
                                           this);

  // Remember which callback and URL correspond to the current fetcher object.
  ClientReportInfo* info = new ClientReportInfo;
  info->callback.swap(cb);  // takes ownership of the callback.
  info->phishing_url = phishing_url;
  client_phishing_reports_[fetcher] = info;

  fetcher->set_load_flags(net::LOAD_DISABLE_CACHE);
  fetcher->set_request_context(request_context_getter_.get());
  fetcher->set_upload_data("application/octet-stream", request_data);
  fetcher->Start();
}

void ClientSideDetectionService::HandleModelResponse(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  if (status.is_success() && RC_REQUEST_OK == response_code) {
    // Copy the model because it has to be accessible after this function
    // returns.  Once we have written the model to a file we will delete the
    // temporary model string. TODO(noelutz): don't store the model to disk if
    // it's invalid.
    tmp_model_string_.reset(new std::string(data));
    base::FileUtilProxy::CreateOrOpenCallback* cb =
        callback_factory_.NewCallback(
            &ClientSideDetectionService::CreateModelFileDone);
    if (!base::FileUtilProxy::CreateOrOpen(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
            model_path_,
            base::PLATFORM_FILE_CREATE_ALWAYS |
            base::PLATFORM_FILE_WRITE |
            base::PLATFORM_FILE_READ,
            cb)) {
      delete cb;
      SetModelStatus(ERROR_STATUS);
    }
  } else {
    SetModelStatus(ERROR_STATUS);
  }
}

void ClientSideDetectionService::HandlePhishingVerdict(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  ClientPhishingResponse response;
  scoped_ptr<ClientReportInfo> info(client_phishing_reports_[source]);
  if (status.is_success() && RC_REQUEST_OK == response_code  &&
      response.ParseFromString(data)) {
    info->callback->Run(info->phishing_url, response.phishy());
  } else {
    DLOG(ERROR) << "Unable to get the server verdict for URL: "
                << info->phishing_url;
    info->callback->Run(info->phishing_url, false);
  }
  client_phishing_reports_.erase(source);
  delete source;
}

}  // namespace safe_browsing
