// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/net/url_fetcher_protect.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "chrome/service/net/service_url_request_context.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"

CloudPrintURLFetcher::CloudPrintURLFetcher()
    : delegate_(NULL),
      protect_entry_(NULL),
      num_retries_(0) {
}

void CloudPrintURLFetcher::StartGetRequest(const GURL& url,
                                           Delegate* delegate,
                                           const std::string& auth_token,
                                           const std::string& retry_policy) {
  StartRequestHelper(url, URLFetcher::GET, delegate, auth_token, retry_policy,
                     std::string(), std::string());
}

void CloudPrintURLFetcher::StartPostRequest(
    const GURL& url,
    Delegate* delegate,
    const std::string& auth_token,
    const std::string& retry_policy,
    const std::string& post_data_mime_type,
    const std::string& post_data) {
  StartRequestHelper(url, URLFetcher::POST, delegate, auth_token, retry_policy,
                     post_data_mime_type, post_data);
}

  // URLFetcher::Delegate implementation.
void CloudPrintURLFetcher::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  VLOG(1) << "CP_PROXY: OnURLFetchComplete, url: " << url
          << ", response code: " << response_code;
  // Make sure we stay alive through the body of this function.
  scoped_refptr<CloudPrintURLFetcher> keep_alive(this);
  ResponseAction action = delegate_->HandleRawResponse(source,
                                                       url,
                                                       status,
                                                       response_code,
                                                       cookies,
                                                       data);
  if (action == CONTINUE_PROCESSING) {
    // If there was an auth error, we are done.
    if (RC_FORBIDDEN == response_code) {
      delegate_->OnRequestAuthError();
      return;
    }
    // We need to retry on all network errors.
    if (!status.is_success() || (response_code != 200))
      action = RETRY_REQUEST;
    else
      action = delegate_->HandleRawData(source, url, data);

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
                                           url,
                                           response_dict,
                                           succeeded);
      else
        action = RETRY_REQUEST;
    }
  }
  // Retry the request if needed.
  if (action == RETRY_REQUEST) {
    int64 back_off_time =
        protect_entry_->UpdateBackoff(URLFetcherProtectEntry::FAILURE);
    ++num_retries_;
    int max_retries = protect_entry_->max_retries();
    if ((-1 != max_retries) && (num_retries_ > max_retries)) {
      // Retry limit reached. Give up.
      delegate_->OnRequestGiveUp();
    } else {
      // Either no retry limit specified or retry limit has not yet been
      // reached. Try again.
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          NewRunnableMethod(this, &CloudPrintURLFetcher::StartRequestNow),
          back_off_time);
    }
  } else {
    protect_entry_->UpdateBackoff(URLFetcherProtectEntry::SUCCESS);
  }
}

void CloudPrintURLFetcher::StartRequestHelper(
    const GURL& url,
    URLFetcher::RequestType request_type,
    Delegate* delegate,
    const std::string& auth_token,
    const std::string& retry_policy,
    const std::string& post_data_mime_type,
    const std::string& post_data) {
  DCHECK(delegate);
  request_.reset(new URLFetcher(url, request_type, this));
  request_->set_request_context(GetRequestContextGetter());
  // Since we implement our own retry logic, disable the retry in URLFetcher.
  request_->set_automatically_retry_on_5xx(false);
  delegate_ = delegate;
  std::string headers = "Authorization: GoogleLogin auth=";
  headers += auth_token;
  headers += "\r\n";
  headers += kChromeCloudPrintProxyHeader;
  request_->set_extra_request_headers(headers);
  if (request_type == URLFetcher::POST) {
    request_->set_upload_data(post_data_mime_type, post_data);
  }
  // Initialize the retry policy for this request.
  protect_entry_ =
      URLFetcherProtectManager::GetInstance()->Register(retry_policy);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(this, &CloudPrintURLFetcher::StartRequestNow),
      protect_entry_->UpdateBackoff(URLFetcherProtectEntry::SEND));
}

void CloudPrintURLFetcher::StartRequestNow() {
  request_->Start();
}

URLRequestContextGetter* CloudPrintURLFetcher::GetRequestContextGetter() {
  ServiceURLRequestContextGetter* getter =
      new ServiceURLRequestContextGetter();
  // Now set up the user agent for cloudprint.
  std::string user_agent = getter->user_agent();
  base::StringAppendF(&user_agent, " %s", kCloudPrintUserAgent);
  getter->set_user_agent(user_agent);
  return getter;
}

