// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_data_fetcher.h"

#include "base/values.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;
using content::UtilityProcessHost;
using content::UtilityProcessHostClient;

namespace {

const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";

}  // namespace

namespace extensions {

class WebstoreDataFetcher::SafeWebstoreResponseParser
    : public UtilityProcessHostClient {
 public:
  SafeWebstoreResponseParser(const base::WeakPtr<WebstoreDataFetcher>& client,
                             const std::string& webstore_data)
      : client_(client),
        webstore_data_(webstore_data) {}

  void Start() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&SafeWebstoreResponseParser::StartWorkOnIOThread, this));
  }

  void StartWorkOnIOThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    UtilityProcessHost* host =
        UtilityProcessHost::Create(
            this, base::MessageLoopProxy::current());
    host->EnableZygote();
    host->Send(new ChromeUtilityMsg_ParseJSON(webstore_data_));
  }

  // Implementing pieces of the UtilityProcessHostClient interface.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(SafeWebstoreResponseParser, message)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Succeeded,
                          OnJSONParseSucceeded)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Failed,
                          OnJSONParseFailed)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnJSONParseSucceeded(const base::ListValue& wrapper) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const Value* value = NULL;
    CHECK(wrapper.Get(0, &value));
    if (value->IsType(Value::TYPE_DICTIONARY)) {
      parsed_webstore_data_.reset(
          static_cast<const DictionaryValue*>(value)->DeepCopy());
    } else {
      error_ = kInvalidWebstoreResponseError;
    }

    ReportResults();
  }

  virtual void OnJSONParseFailed(const std::string& error_message) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    error_ = error_message;
    ReportResults();
  }

  void ReportResults() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SafeWebstoreResponseParser::ReportResultOnUIThread, this));
  }

  void ReportResultOnUIThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!client_)
      return;

    if (error_.empty() && parsed_webstore_data_.get()) {
      client_->OnWebstoreResponseParseSuccess(parsed_webstore_data_.release());
    } else {
      client_->OnWebstoreResponseParseFailure(error_);
    }
  }

 private:
  virtual ~SafeWebstoreResponseParser() {}

  base::WeakPtr<WebstoreDataFetcher> client_;

  std::string webstore_data_;
  std::string error_;
  scoped_ptr<DictionaryValue> parsed_webstore_data_;

  DISALLOW_COPY_AND_ASSIGN(SafeWebstoreResponseParser);
};

WebstoreDataFetcher::WebstoreDataFetcher(
    WebstoreDataFetcherDelegate* delegate,
    net::URLRequestContextGetter* request_context,
    const GURL& referrer_url,
    const std::string webstore_item_id)
    : delegate_(delegate),
      request_context_(request_context),
      referrer_url_(referrer_url),
      id_(webstore_item_id) {
}

WebstoreDataFetcher::~WebstoreDataFetcher() {}

void WebstoreDataFetcher::Start() {
  GURL webstore_data_url(extension_urls::GetWebstoreItemJsonDataURL(id_));

  webstore_data_url_fetcher_.reset(net::URLFetcher::Create(
      webstore_data_url, net::URLFetcher::GET, this));
  webstore_data_url_fetcher_->SetRequestContext(request_context_);
  webstore_data_url_fetcher_->SetReferrer(referrer_url_.spec());
  webstore_data_url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                                           net::LOAD_DISABLE_CACHE);
  webstore_data_url_fetcher_->Start();
}

void WebstoreDataFetcher::OnWebstoreResponseParseSuccess(
    base::DictionaryValue* webstore_data) {
  delegate_->OnWebstoreResponseParseSuccess(webstore_data);
}

void WebstoreDataFetcher::OnWebstoreResponseParseFailure(
    const std::string& error) {
  delegate_->OnWebstoreResponseParseFailure(error);
}

void WebstoreDataFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  CHECK_EQ(webstore_data_url_fetcher_.get(), source);

  scoped_ptr<net::URLFetcher> fetcher(webstore_data_url_fetcher_.Pass());

  if (!fetcher->GetStatus().is_success() ||
      fetcher->GetResponseCode() != 200) {
    delegate_->OnWebstoreRequestFailure();
    return;
  }

  std::string webstore_json_data;
  fetcher->GetResponseAsString(&webstore_json_data);

  scoped_refptr<SafeWebstoreResponseParser> parser =
      new SafeWebstoreResponseParser(AsWeakPtr(), webstore_json_data);
  // The parser will call us back via OnWebstoreResponseParseSucces or
  // OnWebstoreResponseParseFailure.
  parser->Start();
}

}  // namespace extensions
