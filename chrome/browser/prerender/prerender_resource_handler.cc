// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_resource_handler.h"

#include "chrome/browser/net/chrome_url_request_context.h"
#include "content/common/resource_response.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace prerender {

namespace {

bool ShouldPrerenderURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  if (!url.SchemeIs("http")) {
    RecordFinalStatus(FINAL_STATUS_HTTPS);
    return false;
  }
  return true;
}

bool ValidateAliasURLs(const std::vector<GURL>& urls) {
  for (std::vector<GURL>::const_iterator it = urls.begin();
       it != urls.end();
       ++it) {
    if (!ShouldPrerenderURL(*it))
      return false;
  }
  return true;
}

bool ShouldPrerender(const ResourceResponse* response) {
  if (!response)
    return false;
  const ResourceResponseHead& rrh = response->response_head;
  if (!rrh.headers)
    return false;
  if (rrh.mime_type != "text/html")
    return false;
  if (rrh.headers->response_code() != 200)
    return false;
  return true;
}

}  // namespace

PrerenderResourceHandler* PrerenderResourceHandler::MaybeCreate(
    const net::URLRequest& request,
    ChromeURLRequestContext* context,
    ResourceHandler* next_handler,
    bool is_from_prerender,
    int child_id,
    int route_id) {
  if (!context || !context->prerender_manager())
    return NULL;
  if (!(request.load_flags() & net::LOAD_PREFETCH))
    return NULL;
  if (!ShouldPrerenderURL(request.url()))
    return NULL;
  if (request.method() != "GET")
    return NULL;

  return new PrerenderResourceHandler(request,
                                      next_handler,
                                      context->prerender_manager(),
                                      is_from_prerender,
                                      child_id,
                                      route_id);
}

PrerenderResourceHandler::PrerenderResourceHandler(
    const net::URLRequest& request,
    ResourceHandler* next_handler,
    PrerenderManager* prerender_manager,
    bool make_pending,
    int child_id,
    int route_id)
    : next_handler_(next_handler),
      prerender_manager_(prerender_manager),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          prerender_callback_(NewCallback(
              this, &PrerenderResourceHandler::StartPrerender))),
      request_(request),
      child_id_(child_id),
      route_id_(route_id),
      make_pending_(make_pending) {
  DCHECK(next_handler);
  DCHECK(prerender_manager);
}

PrerenderResourceHandler::PrerenderResourceHandler(
    const net::URLRequest& request,
    ResourceHandler* next_handler,
    PrerenderCallback* callback)
    : next_handler_(next_handler),
      prerender_callback_(callback),
      request_(request) {
  DCHECK(next_handler);
  DCHECK(callback);
}

PrerenderResourceHandler::~PrerenderResourceHandler() {
}

bool PrerenderResourceHandler::OnUploadProgress(int request_id,
                                                uint64 position,
                                                uint64 size) {
  return next_handler_->OnUploadProgress(request_id, position, size);
}

bool PrerenderResourceHandler::OnRequestRedirected(int request_id,
                                                   const GURL& url,
                                                   ResourceResponse* response,
                                                   bool* defer) {
  bool will_redirect = next_handler_->OnRequestRedirected(
      request_id, url, response, defer);
  if (will_redirect) {
    if (!ShouldPrerenderURL(url))
      return false;
    alias_urls_.push_back(url);
    url_ = url;
  }
  return will_redirect;
}

bool PrerenderResourceHandler::OnResponseStarted(int request_id,
                                                 ResourceResponse* response) {
  if (ShouldPrerender(response)) {
    DCHECK(ValidateAliasURLs(alias_urls_));
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(
            this,
            &PrerenderResourceHandler::RunCallbackFromUIThread,
            std::make_pair(child_id_, route_id_),
            url_,
            alias_urls_,
            GURL(request_.referrer()),
            make_pending_));
  }
  return next_handler_->OnResponseStarted(request_id, response);
}

bool PrerenderResourceHandler::OnWillStart(int request_id,
                                           const GURL& url,
                                           bool* defer) {
  bool will_start = next_handler_->OnWillStart(request_id, url, defer);
  if (will_start) {
    if (!ShouldPrerenderURL(url))
      return false;
    alias_urls_.push_back(url);
    url_ = url;
  }
  return will_start;
}

bool PrerenderResourceHandler::OnWillRead(int request_id,
                                          net::IOBuffer** buf,
                                          int* buf_size,
                                          int min_size) {
  return next_handler_->OnWillRead(request_id, buf, buf_size, min_size);
}

bool PrerenderResourceHandler::OnReadCompleted(int request_id,
                                               int* bytes_read) {
  return next_handler_->OnReadCompleted(request_id, bytes_read);
}

bool PrerenderResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  return next_handler_->OnResponseCompleted(request_id, status, security_info);
}

void PrerenderResourceHandler::OnRequestClosed() {
  next_handler_->OnRequestClosed();
}

void PrerenderResourceHandler::RunCallbackFromUIThread(
    const std::pair<int, int>& child_route_id_pair,
    const GURL& url,
    const std::vector<GURL>& alias_urls,
    const GURL& referrer,
    bool make_pending) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prerender_callback_->Run(child_route_id_pair,
                           url, alias_urls, referrer,
                           make_pending);
}

void PrerenderResourceHandler::StartPrerender(
    const std::pair<int, int>& child_route_id_pair,
    const GURL& url,
    const std::vector<GURL>& alias_urls,
    const GURL& referrer,
    bool make_pending) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!prerender_manager_->is_enabled())
    return;
  if (make_pending) {
    prerender_manager_->AddPendingPreload(child_route_id_pair,
                                          url, alias_urls, referrer);
  } else {
    prerender_manager_->AddPreload(url, alias_urls, referrer);
  }
}

}  // namespace prerender
