// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/net/http_return.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "chrome/service/net/service_url_request_context.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"

CloudPrintURLFetcher::CloudPrintURLFetcher()
    : delegate_(NULL),
      num_retries_(0) {
}

void CloudPrintURLFetcher::StartGetRequest(
    const GURL& url,
    Delegate* delegate,
    const std::string& auth_token,
    int max_retries,
    const std::string& additional_headers) {
  StartRequestHelper(url, URLFetcher::GET, delegate, auth_token, max_retries,
                     std::string(), std::string(), additional_headers);
}

void CloudPrintURLFetcher::StartPostRequest(
    const GURL& url,
    Delegate* delegate,
    const std::string& auth_token,
    int max_retries,
    const std::string& post_data_mime_type,
    const std::string& post_data,
    const std::string& additional_headers) {
  StartRequestHelper(url, URLFetcher::POST, delegate, auth_token, max_retries,
                     post_data_mime_type, post_data, additional_headers);
}

  // URLFetcher::Delegate implementation.
void CloudPrintURLFetcher::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
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
    // If the response code is greater than or equal to 500, then the back-off
    // period has been increased at the network level; otherwise, explicitly
    // call ReceivedContentWasMalformed() to count the current request as a
    // failure and increase the back-off period.
    if (response_code < 500)
      request_->ReceivedContentWasMalformed();

    ++num_retries_;
    if ((-1 != source->max_retries()) &&
        (num_retries_ > source->max_retries())) {
      // Retry limit reached. Give up.
      delegate_->OnRequestGiveUp();
    } else {
      // Either no retry limit specified or retry limit has not yet been
      // reached. Try again.
      request_->Start();
    }
  }
}

void CloudPrintURLFetcher::StartRequestHelper(
    const GURL& url,
    URLFetcher::RequestType request_type,
    Delegate* delegate,
    const std::string& auth_token,
    int max_retries,
    const std::string& post_data_mime_type,
    const std::string& post_data,
    const std::string& additional_headers) {
  DCHECK(delegate);
  request_.reset(new URLFetcher(url, request_type, this));
  request_->set_request_context(GetRequestContextGetter());
  // Since we implement our own retry logic, disable the retry in URLFetcher.
  request_->set_automatically_retry_on_5xx(false);
  request_->set_max_retries(max_retries);
  delegate_ = delegate;
  std::string headers = "Authorization: GoogleLogin auth=";
  headers += auth_token;
  headers += "\r\n";
  headers += kChromeCloudPrintProxyHeader;
  if (!additional_headers.empty()) {
    headers += "\r\n";
    headers += additional_headers;
  }
  request_->set_extra_request_headers(headers);
  if (request_type == URLFetcher::POST) {
    request_->set_upload_data(post_data_mime_type, post_data);
  }

  request_->Start();
}

CloudPrintURLFetcher::~CloudPrintURLFetcher() {}

URLRequestContextGetter* CloudPrintURLFetcher::GetRequestContextGetter() {
  ServiceURLRequestContextGetter* getter =
      new ServiceURLRequestContextGetter();
  // Now set up the user agent for cloudprint.
  std::string user_agent = getter->user_agent();
  base::StringAppendF(&user_agent, " %s", kCloudPrintUserAgent);
  getter->set_user_agent(user_agent);
  return getter;
}

