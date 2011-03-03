// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/gaia/service_gaia_authenticator.h"

#include "base/message_loop_proxy.h"
#include "chrome/service/net/service_url_request_context.h"
#include "chrome/service/service_process.h"
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

ServiceGaiaAuthenticator::~ServiceGaiaAuthenticator() {
}

bool ServiceGaiaAuthenticator::Post(const GURL& url,
                                    const std::string& post_body,
                                    unsigned long* response_code,
                                    std::string* response_body) {
  DCHECK(url.SchemeIsSecure());
  DCHECK(io_message_loop_proxy_);
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ServiceGaiaAuthenticator::DoPost, url,
                        post_body));
  // TODO(sanjeevr): Waiting here until the network request completes is not
  // desirable. We need to change Post to be asynchronous.
  if (!http_post_completed_.Wait())  // Block until network request completes.
    NOTREACHED();                    // See OnURLFetchComplete.

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

void ServiceGaiaAuthenticator::DoPost(const GURL& post_url,
                                      const std::string& post_body) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  URLFetcher* request = new URLFetcher(post_url, URLFetcher::POST, this);
  request->set_request_context(
      g_service_process->GetServiceURLRequestContextGetter());
  request->set_upload_data("application/x-www-form-urlencoded", post_body);
  request->Start();
}

// URLFetcher::Delegate implementation
void ServiceGaiaAuthenticator::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  http_response_code_ = response_code;
  response_data_ = data;
  delete source;
  // Add an extra reference because we want http_post_completed_ to remain
  // valid until after Signal() returns.
  scoped_refptr<ServiceGaiaAuthenticator> keep_alive(this);
  // Wake the blocked thread in Post.
  http_post_completed_.Signal();
  // WARNING: DONT DO ANYTHING AFTER THIS CALL! |this| may be deleted!
}
