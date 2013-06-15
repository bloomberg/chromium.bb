// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/two_phase_uploader.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/task_runner.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"

namespace {

// Header sent on initial request to start the two phase upload process.
const char* kStartHeader = "x-goog-resumable: start";

// Header returned on initial response with URL to use for the second phase.
const char* kLocationHeader = "Location";

const char* kUploadContentType = "application/octet-stream";

class TwoPhaseUploaderImpl : public net::URLFetcherDelegate,
                             public TwoPhaseUploader {
 public:
  TwoPhaseUploaderImpl(net::URLRequestContextGetter* url_request_context_getter,
                       base::TaskRunner* file_task_runner,
                       const GURL& base_url,
                       const std::string& metadata,
                       const base::FilePath& file_path,
                       const ProgressCallback& progress_callback,
                       const FinishCallback& finish_callback);
  virtual ~TwoPhaseUploaderImpl();

  // Begins the upload process.
  virtual void Start() OVERRIDE;

  // net::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  virtual void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                        int64 current,
                                        int64 total) OVERRIDE;

 private:
  void UploadMetadata();
  void UploadFile();
  void Finish(int net_error, int response_code, const std::string& response);

  State state_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  scoped_refptr<base::TaskRunner> file_task_runner_;
  GURL base_url_;
  GURL upload_url_;
  std::string metadata_;
  const base::FilePath file_path_;
  ProgressCallback progress_callback_;
  FinishCallback finish_callback_;

  scoped_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(TwoPhaseUploaderImpl);
};

TwoPhaseUploaderImpl::TwoPhaseUploaderImpl(
    net::URLRequestContextGetter* url_request_context_getter,
    base::TaskRunner* file_task_runner,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& file_path,
    const ProgressCallback& progress_callback,
    const FinishCallback& finish_callback)
    : state_(STATE_NONE),
      url_request_context_getter_(url_request_context_getter),
      file_task_runner_(file_task_runner),
      base_url_(base_url),
      metadata_(metadata),
      file_path_(file_path),
      progress_callback_(progress_callback),
      finish_callback_(finish_callback) {
}

TwoPhaseUploaderImpl::~TwoPhaseUploaderImpl() {
  DCHECK(CalledOnValidThread());
}

void TwoPhaseUploaderImpl::Start() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(STATE_NONE, state_);

  UploadMetadata();
}

void TwoPhaseUploaderImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  net::URLRequestStatus status = source->GetStatus();
  int response_code = source->GetResponseCode();

  DVLOG(1) << __FUNCTION__ << " " << source->GetURL().spec()
           << " " << status.status() << " " << response_code;

  if (!status.is_success()) {
    LOG(ERROR) << "URLFetcher failed, status=" << status.status()
               << " err=" << status.error();
    Finish(status.error(), response_code, std::string());
    return;
  }

  std::string response;
  source->GetResponseAsString(&response);

  switch (state_) {
    case UPLOAD_METADATA:
      {
        if (response_code != 201) {
          LOG(ERROR) << "Invalid response to initial request: "
                     << response_code;
          Finish(net::OK, response_code, response);
          return;
        }
        std::string location;
        if (!source->GetResponseHeaders()->EnumerateHeader(
              NULL, kLocationHeader, &location)) {
          LOG(ERROR) << "no location header";
          Finish(net::OK, response_code, std::string());
          return;
        }
        DVLOG(1) << "upload location: " << location;
        upload_url_ = GURL(location);
        UploadFile();
        break;
      }
    case UPLOAD_FILE:
      if (response_code != 200) {
          LOG(ERROR) << "Invalid response to upload request: "
                     << response_code;
      } else {
        state_ = STATE_SUCCESS;
      }
      Finish(net::OK, response_code, response);
      return;
    default:
      NOTREACHED();
  };
}

void TwoPhaseUploaderImpl::OnURLFetchUploadProgress(
    const net::URLFetcher* source,
    int64 current,
    int64 total) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << __FUNCTION__ << " " << source->GetURL().spec()
           << " " << current << "/" << total;
  if (state_ == UPLOAD_FILE && !progress_callback_.is_null())
    progress_callback_.Run(current, total);
}

void TwoPhaseUploaderImpl::UploadMetadata() {
  DCHECK(CalledOnValidThread());
  state_ = UPLOAD_METADATA;
  url_fetcher_.reset(net::URLFetcher::Create(base_url_, net::URLFetcher::POST,
                                             this));
  url_fetcher_->SetRequestContext(url_request_context_getter_.get());
  url_fetcher_->SetExtraRequestHeaders(kStartHeader);
  url_fetcher_->SetUploadData(kUploadContentType, metadata_);
  url_fetcher_->Start();
}

void TwoPhaseUploaderImpl::UploadFile() {
  DCHECK(CalledOnValidThread());
  state_ = UPLOAD_FILE;

  url_fetcher_.reset(net::URLFetcher::Create(upload_url_, net::URLFetcher::PUT,
                                             this));
  url_fetcher_->SetRequestContext(url_request_context_getter_.get());
  url_fetcher_->SetUploadFilePath(
      kUploadContentType, file_path_, 0, kuint64max, file_task_runner_);
  url_fetcher_->Start();
}

void TwoPhaseUploaderImpl::Finish(int net_error,
                                  int response_code,
                                  const std::string& response) {
  DCHECK(CalledOnValidThread());
  finish_callback_.Run(state_, net_error, response_code, response);
}

}  // namespace

// static
TwoPhaseUploaderFactory* TwoPhaseUploader::factory_ = NULL;

// static
TwoPhaseUploader* TwoPhaseUploader::Create(
    net::URLRequestContextGetter* url_request_context_getter,
    base::TaskRunner* file_task_runner,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& file_path,
    const ProgressCallback& progress_callback,
    const FinishCallback& finish_callback) {
  if (!TwoPhaseUploader::factory_)
    return new TwoPhaseUploaderImpl(
        url_request_context_getter, file_task_runner, base_url, metadata,
        file_path, progress_callback, finish_callback);
  return TwoPhaseUploader::factory_->CreateTwoPhaseUploader(
      url_request_context_getter, file_task_runner, base_url, metadata,
      file_path, progress_callback, finish_callback);
}
