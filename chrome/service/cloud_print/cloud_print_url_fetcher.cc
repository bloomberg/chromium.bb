// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"

#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/net/http_return.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/cloud_print_token_store.h"
#include "chrome/service/net/service_url_request_context.h"
#include "chrome/service/service_process.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"

CloudPrintURLFetcher::CloudPrintURLFetcher()
    : delegate_(NULL),
      num_retries_(0) {
}

void CloudPrintURLFetcher::StartGetRequest(
    const GURL& url,
    Delegate* delegate,
    int max_retries,
    const std::string& additional_headers) {
  StartRequestHelper(url,
                     URLFetcher::GET,
                     delegate,
                     max_retries,
                     std::string(),
                     std::string(),
                     additional_headers);
}

void CloudPrintURLFetcher::StartPostRequest(
    const GURL& url,
    Delegate* delegate,
    int max_retries,
    const std::string& post_data_mime_type,
    const std::string& post_data,
    const std::string& additional_headers) {
  StartRequestHelper(url,
                     URLFetcher::POST,
                     delegate,
                     max_retries,
                     post_data_mime_type,
                     post_data,
                     additional_headers);
}

void CloudPrintURLFetcher::OnURLFetchComplete(
    const content::URLFetcher* source) {
  VLOG(1) << "CP_PROXY: OnURLFetchComplete, url: " << source->GetUrl()
          << ", response code: " << source->GetResponseCode();
  // Make sure we stay alive through the body of this function.
  scoped_refptr<CloudPrintURLFetcher> keep_alive(this);
  std::string data;
  source->GetResponseAsString(&data);
  ResponseAction action = delegate_->HandleRawResponse(
      source,
      source->GetUrl(),
      source->GetStatus(),
      source->GetResponseCode(),
      source->GetCookies(),
      data);
  if (action == CONTINUE_PROCESSING) {
    // If we are not using an OAuth token, and we got an auth error, we are
    // done. Else, the token may have been refreshed. Let us try again.
    if ((RC_FORBIDDEN == source->GetResponseCode()) &&
        (!CloudPrintTokenStore::current() ||
         !CloudPrintTokenStore::current()->token_is_oauth())) {
      delegate_->OnRequestAuthError();
      return;
    }
    // We need to retry on all network errors.
    if (!source->GetStatus().is_success() || (source->GetResponseCode() != 200))
      action = RETRY_REQUEST;
    else
      action = delegate_->HandleRawData(source, source->GetUrl(), data);

    if (action == CONTINUE_PROCESSING) {
      // If the delegate is not interested in handling the raw response data,
      // we assume that a JSON response is expected. If we do not get a JSON
      // response, we will retry (to handle the case where we got redirected
      // to a non-cloudprint-server URL eg. for authentication).
      bool succeeded = false;
      DictionaryValue* response_dict = NULL;
      CloudPrintHelpers::ParseResponseJSON(data, &succeeded, &response_dict);
      if (response_dict)
        action = delegate_->HandleJSONData(source,
                                           source->GetUrl(),
                                           response_dict,
                                           succeeded);
      else
        action = RETRY_REQUEST;
    }
  }
  // Retry the request if needed.
  if (action == RETRY_REQUEST) {
    // Explicitly call ReceivedContentWasMalformed() to ensure the current
    // request gets counted as a failure for calculation of the back-off
    // period.  If it was already a failure by status code, this call will
    // be ignored.
    request_->ReceivedContentWasMalformed();

    ++num_retries_;
    if ((-1 != source->GetMaxRetries()) &&
        (num_retries_ > source->GetMaxRetries())) {
      // Retry limit reached. Give up.
      delegate_->OnRequestGiveUp();
    } else {
      // Either no retry limit specified or retry limit has not yet been
      // reached. Try again. Set up the request headers again because the token
      // may have changed.
      SetupRequestHeaders();
      request_->StartWithRequestContextGetter(GetRequestContextGetter());
    }
  }
}

void CloudPrintURLFetcher::StartRequestHelper(
    const GURL& url,
    URLFetcher::RequestType request_type,
    Delegate* delegate,
    int max_retries,
    const std::string& post_data_mime_type,
    const std::string& post_data,
    const std::string& additional_headers) {
  DCHECK(delegate);
  // Persist the additional headers in case we need to retry the request.
  additional_headers_ = additional_headers;
  request_.reset(new URLFetcher(url, request_type, this));
  request_->SetRequestContext(GetRequestContextGetter());
  // Since we implement our own retry logic, disable the retry in URLFetcher.
  request_->SetAutomaticallyRetryOn5xx(false);
  request_->SetMaxRetries(max_retries);
  SetupRequestHeaders();
  delegate_ = delegate;
  if (request_type == URLFetcher::POST) {
    request_->SetUploadData(post_data_mime_type, post_data);
  }

  request_->Start();
}

void CloudPrintURLFetcher::SetupRequestHeaders() {
  std::string headers;
  CloudPrintTokenStore* token_store = CloudPrintTokenStore::current();
  if (token_store) {
    headers = token_store->token_is_oauth() ?
        "Authorization: OAuth " : "Authorization: GoogleLogin auth=";
    headers += token_store->token();
    headers += "\r\n";
  }
  headers += kChromeCloudPrintProxyHeader;
  if (!additional_headers_.empty()) {
    headers += "\r\n";
    headers += additional_headers_;
  }
  request_->SetExtraRequestHeaders(headers);
}

CloudPrintURLFetcher::~CloudPrintURLFetcher() {}

net::URLRequestContextGetter* CloudPrintURLFetcher::GetRequestContextGetter() {
  ServiceURLRequestContextGetter* getter =
      g_service_process->GetServiceURLRequestContextGetter();
  // Now set up the user agent for cloudprint.
  std::string user_agent = getter->user_agent();
  base::StringAppendF(&user_agent, " %s", kCloudPrintUserAgent);
  getter->set_user_agent(user_agent);
  return getter;
}
