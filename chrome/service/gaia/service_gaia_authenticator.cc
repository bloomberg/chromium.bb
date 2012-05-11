// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/gaia/service_gaia_authenticator.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "chrome/service/net/service_url_request_context.h"
#include "chrome/service/service_process.h"
#include "content/public/common/url_fetcher.h"
#include "googleurl/src/gurl.h"

ServiceGaiaAuthenticator::ServiceGaiaAuthenticator(
    const std::string& user_agent, const std::string& service_id,
    const std::string& gaia_url,
    base::MessageLoopProxy* io_message_loop_proxy)
        : gaia::GaiaAuthenticator(user_agent, service_id, gaia_url),
          http_post_completed_(false, false),
          io_message_loop_proxy_(io_message_loop_proxy),
          http_response_code_(0) {
}

// content::URLFetcherDelegate implementation
void ServiceGaiaAuthenticator::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  http_response_code_ = source->GetResponseCode();
  source->GetResponseAsString(&response_data_);
  delete source;
  // Add an extra reference because we want http_post_completed_ to remain
  // valid until after Signal() returns.
  scoped_refptr<ServiceGaiaAuthenticator> keep_alive(this);
  // Wake the blocked thread in Post.
  http_post_completed_.Signal();
  // WARNING: DONT DO ANYTHING AFTER THIS CALL! |this| may be deleted!
}

bool ServiceGaiaAuthenticator::Post(const GURL& url,
                                    const std::string& post_body,
                                    unsigned long* response_code,
                                    std::string* response_body) {
  DCHECK(url.SchemeIsSecure());
  DCHECK(io_message_loop_proxy_);
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&ServiceGaiaAuthenticator::DoPost, this, url, post_body));
  // TODO(sanjeevr): Waiting here until the network request completes is not
  // desirable. We need to change Post to be asynchronous.
  // Block until network request completes. See OnURLFetchComplete.
  http_post_completed_.Wait();

  *response_code = static_cast<int>(http_response_code_);
  *response_body = response_data_;
  return true;
}

// TODO(sanjeevr): This is a placeholder implementation. Need to move this logic
// to a common location within the service process so that it can be resued by
// other classes needing a backoff delay calculation.
int ServiceGaiaAuthenticator::GetBackoffDelaySeconds(
    int current_backoff_delay) {
  const int kMaxBackoffDelaySeconds = 60 * 60;  // (1 hour)
  int ret = 0;
  if (0 == current_backoff_delay) {
    ret = 1;
  } else {
    ret = current_backoff_delay * 2;
  }
  if (ret > kMaxBackoffDelaySeconds) {
    ret = kMaxBackoffDelaySeconds;
  }
  return ret;
}

ServiceGaiaAuthenticator::~ServiceGaiaAuthenticator() {}

void ServiceGaiaAuthenticator::DoPost(const GURL& post_url,
                                      const std::string& post_body) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  content::URLFetcher* request = content::URLFetcher::Create(
      post_url, content::URLFetcher::POST, this);
  request->SetRequestContext(
      g_service_process->GetServiceURLRequestContextGetter());
  request->SetUploadData("application/x-www-form-urlencoded", post_body);
  request->Start();
}
