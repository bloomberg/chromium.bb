// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/prerender_interceptor.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"

DISABLE_RUNNABLE_METHOD_REFCOUNT(chrome_browser_net::PrerenderInterceptor);

namespace chrome_browser_net {

PrerenderInterceptor::PrerenderInterceptor()
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        callback_(NewCallback(this,
                              &PrerenderInterceptor::PrerenderDispatch))) {
  URLRequest::RegisterRequestInterceptor(this);
}

PrerenderInterceptor::PrerenderInterceptor(
    PrerenderInterceptorCallback* callback)
  : callback_(callback) {
  URLRequest::RegisterRequestInterceptor(this);
}

PrerenderInterceptor::~PrerenderInterceptor() {
  URLRequest::UnregisterRequestInterceptor(this);
}

net::URLRequestJob* PrerenderInterceptor::MaybeIntercept(
    net::URLRequest* request) {
  return NULL;
}

net::URLRequestJob* PrerenderInterceptor::MaybeInterceptResponse(
    net::URLRequest* request) {
  // TODO(gavinp): unfortunately, we can't figure out the origin
  // of this request here on systems where the referrer is blocked by
  // configuration.

  // TODO(gavinp): note that the response still might be intercepted
  // by a later interceptor.  Should we write an interposing delegate
  // and only prerender dispatch on requests that aren't intercepted?
  // Or is this a slippery slope?

  if (request->load_flags() & net::LOAD_PREFETCH) {
    std::string mime_type;
    request->GetMimeType(&mime_type);
    if (mime_type == "text/html")
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          NewRunnableMethod(this,
                            &PrerenderInterceptor::RunCallbackFromUIThread,
                            request->url()));
  }
  return NULL;
}

void PrerenderInterceptor::RunCallbackFromUIThread(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback_->Run(url);
}

void PrerenderInterceptor::PrerenderDispatch(
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(2) << "PrerenderDispatchOnUIThread: url=" << url;
}

}  // namespace chrome_browser_net

