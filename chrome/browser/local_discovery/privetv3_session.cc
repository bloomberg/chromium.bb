// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv3_session.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"

namespace local_discovery {

namespace {

const char kUrlPlaceHolder[] = "http://host/";

const char kStubPrivetCode[] = "1234";

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
                  const PrivetV3Session::MessageCallback& callback);
  ~FetcherDelegate() override;

  // PrivetURLFetcher::Delegate methods.
  void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) override;
  void OnError(PrivetURLFetcher* fetcher,
               PrivetURLFetcher::ErrorType error) override;
  void OnParsedJson(PrivetURLFetcher* fetcher,
                    const base::DictionaryValue& value,
                    bool has_error) override;

 private:
  friend class PrivetV3Session;
  void DeleteThis();

  scoped_ptr<PrivetURLFetcher> url_fetcher_;
  base::WeakPtr<PrivetV3Session> session_;
  MessageCallback callback_;
};

PrivetV3Session::FetcherDelegate::FetcherDelegate(
    const base::WeakPtr<PrivetV3Session>& session,
    const PrivetV3Session::MessageCallback& callback)
    : session_(session), callback_(callback) {
}

PrivetV3Session::FetcherDelegate::~FetcherDelegate() {
}

void PrivetV3Session::FetcherDelegate::OnNeedPrivetToken(
    PrivetURLFetcher* fetcher,
    const PrivetURLFetcher::TokenCallback& callback) {
  NOTREACHED();
}

void PrivetV3Session::FetcherDelegate::OnError(
    PrivetURLFetcher* fetcher,
    PrivetURLFetcher::ErrorType error) {
  if (session_) {
    DeleteThis();
    callback_.Run(Result::STATUS_CONNECTIONERROR, base::DictionaryValue());
  }
}

void PrivetV3Session::FetcherDelegate::OnParsedJson(
    PrivetURLFetcher* fetcher,
    const base::DictionaryValue& value,
    bool has_error) {
  if (session_) {
    DeleteThis();
    callback_.Run(
        has_error ? Result::STATUS_SETUPERROR : Result::STATUS_SUCCESS, value);
  }
}

void PrivetV3Session::FetcherDelegate::DeleteThis() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&PrivetV3Session::DeleteFetcher, session_,
                            base::Unretained(this)));
}

PrivetV3Session::Request::Request() {
}

PrivetV3Session::Request::~Request() {
}

PrivetV3Session::PrivetV3Session(scoped_ptr<PrivetHTTPClient> client)
    : client_(client.Pass()), code_confirmed_(false), weak_ptr_factory_(this) {
}

PrivetV3Session::~PrivetV3Session() {
}

void PrivetV3Session::Init(const InitCallback& callback) {
  // TODO: call /info.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PrivetV3Session::RunCallback, weak_ptr_factory_.GetWeakPtr(),
                 base::Bind(callback, Result::STATUS_SUCCESS,
                            std::vector<PairingType>(
                                1, PairingType::PAIRING_TYPE_EMBEDDEDCODE))),
      base::TimeDelta::FromSeconds(1));
}

void PrivetV3Session::StartPairing(PairingType pairing_type,
                                   const ResultCallback& callback) {
  // TODO: call /privet/v3/pairing/start.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PrivetV3Session::RunCallback, weak_ptr_factory_.GetWeakPtr(),
                 base::Bind(callback, Result::STATUS_SUCCESS)),
      base::TimeDelta::FromSeconds(1));
}

void PrivetV3Session::ConfirmCode(const std::string& code,
                                  const ResultCallback& callback) {
  // TODO: call /privet/v3/pairing/confirm.
  if (code == kStubPrivetCode) {
    code_confirmed_ = true;
    callback.Run(Result::STATUS_SUCCESS);
  } else {
    callback.Run(Result::STATUS_BADPAIRINGCODEERROR);
  }
}

void PrivetV3Session::SendMessage(const std::string& api,
                                  const base::DictionaryValue& input,
                                  const MessageCallback& callback) {
  if (!code_confirmed_)
    return callback.Run(Result::STATUS_SESSIONERROR, base::DictionaryValue());

  FetcherDelegate* fetcher_delegate(
      new FetcherDelegate(weak_ptr_factory_.GetWeakPtr(), callback));
  fetchers_.push_back(fetcher_delegate);

  scoped_ptr<PrivetURLFetcher> url_fetcher(client_->CreateURLFetcher(
      CreatePrivetURL(api), net::URLFetcher::POST, fetcher_delegate));

  std::string json;
  base::JSONWriter::WriteWithOptions(
      &input, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  url_fetcher->SetUploadData(cloud_print::kContentTypeJSON, json);

  fetcher_delegate->url_fetcher_ = url_fetcher.Pass();
  fetcher_delegate->url_fetcher_->V3Mode();
  fetcher_delegate->url_fetcher_->Start();
}

void PrivetV3Session::RunCallback(const base::Closure& callback) {
  callback.Run();
}

void PrivetV3Session::DeleteFetcher(const FetcherDelegate* fetcher) {
  fetchers_.erase(std::find(fetchers_.begin(), fetchers_.end(), fetcher));
}

}  // namespace local_discovery
