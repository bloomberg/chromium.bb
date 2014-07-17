// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv3_session.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"

namespace local_discovery {

namespace {

const char kUrlPlaceHolder[] = "http://host/";

GURL CreatePrivetURL(const std::string& path) {
  GURL url(kUrlPlaceHolder);
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return url.ReplaceComponents(replacements);
}

}  // namespace

class PrivetV3Session::FetcherDelegate : public PrivetURLFetcher::Delegate {
 public:
  FetcherDelegate(const base::WeakPtr<PrivetV3Session>& session,
                  Request* request);
  virtual ~FetcherDelegate();

  // PrivetURLFetcher::Delegate methods.
  virtual void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) OVERRIDE;
  virtual void OnError(PrivetURLFetcher* fetcher,
                       PrivetURLFetcher::ErrorType error) OVERRIDE;
  virtual void OnParsedJson(PrivetURLFetcher* fetcher,
                            const base::DictionaryValue& value,
                            bool has_error) OVERRIDE;

 private:
  friend class PrivetV3Session;
  scoped_ptr<PrivetURLFetcher> url_fetcher_;
  base::WeakPtr<PrivetV3Session> session_;
  Request* request_;
};

PrivetV3Session::FetcherDelegate::FetcherDelegate(
    const base::WeakPtr<PrivetV3Session>& session,
    Request* request)
    : session_(session), request_(request) {
}

PrivetV3Session::FetcherDelegate::~FetcherDelegate() {
}

void PrivetV3Session::FetcherDelegate::OnNeedPrivetToken(
    PrivetURLFetcher* fetcher,
    const PrivetURLFetcher::TokenCallback& callback) {
  if (session_)
    session_->client_->RefreshPrivetToken(callback);
}

void PrivetV3Session::FetcherDelegate::OnError(
    PrivetURLFetcher* fetcher,
    PrivetURLFetcher::ErrorType error) {
  request_->OnError(error);
}

void PrivetV3Session::FetcherDelegate::OnParsedJson(
    PrivetURLFetcher* fetcher,
    const base::DictionaryValue& value,
    bool has_error) {
  request_->OnParsedJson(value, has_error);
}

PrivetV3Session::Delegate::~Delegate() {
}

PrivetV3Session::Request::Request() {
}

PrivetV3Session::Request::~Request() {
}

PrivetV3Session::PrivetV3Session(scoped_ptr<PrivetHTTPClient> client,
                                 Delegate* delegate)
    : delegate_(delegate),
      client_(client.Pass()),
      code_confirmed_(false),
      weak_ptr_factory_(this) {
}

PrivetV3Session::~PrivetV3Session() {
}

void PrivetV3Session::Start() {
  delegate_->OnSetupConfirmationNeeded("01234");
}

void PrivetV3Session::ConfirmCode() {
  code_confirmed_ = true;
  delegate_->OnSessionEstablished();
}

void PrivetV3Session::StartRequest(Request* request) {
  CHECK(code_confirmed_);

  request->fetcher_delegate_.reset(
      new FetcherDelegate(weak_ptr_factory_.GetWeakPtr(), request));

  scoped_ptr<PrivetURLFetcher> url_fetcher =
      client_->CreateURLFetcher(CreatePrivetURL(request->GetName()),
                                net::URLFetcher::POST,
                                request->fetcher_delegate_.get());
  std::string json;
  base::JSONWriter::WriteWithOptions(
      &request->GetInput(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  url_fetcher->SetUploadData(cloud_print::kContentTypeJSON, json);

  request->fetcher_delegate_->url_fetcher_ = url_fetcher.Pass();
  request->fetcher_delegate_->url_fetcher_->Start();
}

}  // namespace local_discovery
