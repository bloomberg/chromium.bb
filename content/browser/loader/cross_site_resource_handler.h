// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_CROSS_SITE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_CROSS_SITE_RESOURCE_HANDLER_H_

#include "content/browser/loader/layered_resource_handler.h"
#include "net/url_request/url_request_status.h"

namespace net {
class URLRequest;
}

namespace content {

// Ensures that cross-site responses are delayed until the onunload handler of
// the previous page is allowed to run.  This handler wraps an
// AsyncEventHandler, and it sits inside SafeBrowsing and Buffered event
// handlers.  This is important, so that it can intercept OnResponseStarted
// after we determine that a response is safe and not a download.
class CrossSiteResourceHandler : public LayeredResourceHandler {
 public:
  CrossSiteResourceHandler(scoped_ptr<ResourceHandler> next_handler,
                           int render_process_host_id,
                           int render_view_id,
                           net::URLRequest* request);
  virtual ~CrossSiteResourceHandler();

  // ResourceHandler implementation:
  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& new_url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response,
                                 bool* defer) OVERRIDE;
  virtual bool OnReadCompleted(int request_id,
                               int bytes_read,
                               bool* defer) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;

  // We can now send the response to the new renderer, which will cause
  // WebContentsImpl to swap in the new renderer and destroy the old one.
  void ResumeResponse();

 private:
  // Prepare to render the cross-site response in a new RenderViewHost, by
  // telling the old RenderViewHost to run its onunload handler.
  void StartCrossSiteTransition(
      int request_id,
      ResourceResponse* response);

  void ResumeIfDeferred();

  int render_process_host_id_;
  int render_view_id_;
  net::URLRequest* request_;
  bool has_started_response_;
  bool in_cross_site_transition_;
  int request_id_;
  bool completed_during_transition_;
  bool did_defer_;
  net::URLRequestStatus completed_status_;
  std::string completed_security_info_;
  ResourceResponse* response_;

  DISALLOW_COPY_AND_ASSIGN(CrossSiteResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_CROSS_SITE_RESOURCE_HANDLER_H_
