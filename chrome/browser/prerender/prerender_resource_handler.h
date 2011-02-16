// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/renderer_host/resource_handler.h"

class ChromeURLRequestContext;
namespace net {
class URLRequest;
}

namespace prerender {

// The PrerenderResourceHandler initiates prerendering of web pages
// under the following conditions:
//   - The profile which initiated the request allows prerendering.
//   - The initial request is a GET for a PREFETCH resource type.
//   - The final URL (after redirects) has a scheme of http or https.
//   - The response status code is a 200.
//   - The MIME type of the response (sniffed or explicit) is text/html.
class PrerenderResourceHandler : public ResourceHandler {
 public:
  // Creates a new PrerenderResourceHandler if appropriate for the
  // given |request| and |context|, otherwise NULL is returned. The
  // caller is resposible for deleting the returned handler.
  //
  // |next_handler| is the backup handler that this handler delegates to
  // for the majority of the commands, and must be non-NULL.
  static PrerenderResourceHandler* MaybeCreate(
      const net::URLRequest& request,
      ChromeURLRequestContext* context,
      ResourceHandler* next_handler);

  // OnResponseStarted will ask the |prerender_manager_| to start
  // prerendering the requested resource if it is of an appropriate
  // content type. The next handler is still invoked.
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response);

  // The following methods simply delegate to the next_handler.
  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size);
  virtual bool OnRequestRedirected(int request_id, const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer);
  virtual bool OnWillStart(int request_id, const GURL& url, bool* defer);

  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size);

  virtual bool OnReadCompleted(int request_id, int* bytes_read);

  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info);

  virtual void OnRequestClosed();

 private:
  friend class PrerenderResourceHandlerTest;
  typedef Callback3<const GURL&,
                    const std::vector<GURL>&,
                    const GURL&>::Type PrerenderCallback;

  PrerenderResourceHandler(const net::URLRequest& request,
                           ResourceHandler* next_handler,
                           PrerenderManager* prerender_manager);

  // This constructor is only used from unit tests.
  PrerenderResourceHandler(const net::URLRequest& request,
                           ResourceHandler* next_handler,
                           PrerenderCallback* callback);

  virtual ~PrerenderResourceHandler();

  void RunCallbackFromUIThread(const GURL& url,
                               const std::vector<GURL>& alias_urls,
                               const GURL& referrer);
  void StartPrerender(const GURL& url,
                      const std::vector<GURL>& alias_urls,
                      const GURL& referrer);

  // The set of URLs that are aliases to the URL to be prerendered,
  // as a result of redirects.
  std::vector<GURL> alias_urls_;
  GURL url_;
  scoped_refptr<ResourceHandler> next_handler_;
  scoped_refptr<PrerenderManager> prerender_manager_;
  scoped_ptr<PrerenderCallback> prerender_callback_;

  // Used to obtain the referrer, but only after any redirections occur, as they
  // can result in the referrer being cleared.
  const net::URLRequest& request_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderResourceHandler);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_HANDLER_H_
