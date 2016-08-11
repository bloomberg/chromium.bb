// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_request_handler.h"

#include "components/offline_pages/offline_page_feature.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_interceptor.h"

namespace offline_pages {

namespace {

int kUserDataKey;  // Only address is used.

// An interceptor to hijack requests and potentially service them based on
// their offline information.
class OfflinePageRequestInterceptor : public net::URLRequestInterceptor {
 public:
  explicit OfflinePageRequestInterceptor(void* profile_id)
      : profile_id_(profile_id) {
    DCHECK(profile_id);
  }
  ~OfflinePageRequestInterceptor() override {}

 private:
  // Overrides from net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    OfflinePageRequestHandler* handler = OfflinePageRequestHandler::GetHandler(
        request);
    if (!handler)
      return nullptr;
    return handler->MaybeCreateJob(request, network_delegate, profile_id_);
  }

  // The profile for processing offline pages.
  void* const profile_id_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestInterceptor);
};

}  // namespace

// static
void OfflinePageRequestHandler::InitializeHandler(
    net::URLRequest* request,
    content::ResourceType resource_type) {
  if (!IsOfflinePagesEnabled())
    return;

  // Ignore the requests not for the main resource.
  if (resource_type != content::RESOURCE_TYPE_MAIN_FRAME)
    return;

  // Ignore non-http/https requests.
  if (!request->url().SchemeIsHTTPOrHTTPS())
    return;

  // Ignore requests other than GET.
  if (request->method() != "GET")
    return;

  // Any more precise checks to see if the request should be intercepted are
  // asynchronous, so just create our handler in all cases.
  request->SetUserData(&kUserDataKey, new OfflinePageRequestHandler());
}

// static
OfflinePageRequestHandler* OfflinePageRequestHandler::GetHandler(
    net::URLRequest* request) {
  return static_cast<OfflinePageRequestHandler*>(
      request->GetUserData(&kUserDataKey));
}

// static
std::unique_ptr<net::URLRequestInterceptor>
OfflinePageRequestHandler::CreateInterceptor(void* profile_id) {
  if (!IsOfflinePagesEnabled())
    return nullptr;

  return std::unique_ptr<net::URLRequestInterceptor>(
      new OfflinePageRequestInterceptor(profile_id));
}

OfflinePageRequestHandler::OfflinePageRequestHandler() {
}

OfflinePageRequestHandler::~OfflinePageRequestHandler() {
}

net::URLRequestJob* OfflinePageRequestHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    void* profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // TODO(jianli): to be implemented.
  return nullptr;
}

}  // namespace offline_pages
