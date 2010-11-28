// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PRERENDER_INTERCEPTOR_H_
#define CHROME_BROWSER_NET_PRERENDER_INTERCEPTOR_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "net/url_request/url_request.h"

class GURL;

namespace chrome_browser_net {

// The PrerenderInterceptor watches prefetch requests, and when
// they are for type text/html, notifies the prerendering
// system about the fetch so it may consider the URL.
class PrerenderInterceptor : public URLRequest::Interceptor {
 public:
  PrerenderInterceptor();
  virtual ~PrerenderInterceptor();

  // URLRequest::Interceptor overrides.  We only care about
  // MaybeInterceptResponse, but must capture MaybeIntercept since
  // it is pure virtual.
  virtual net::URLRequestJob* MaybeIntercept(net::URLRequest* request);
  virtual net::URLRequestJob* MaybeInterceptResponse(net::URLRequest* request);

 private:
  friend class PrerenderInterceptorTest;

  typedef Callback1<const GURL&>::Type PrerenderInterceptorCallback;

  // This constructor is provided for the unit test only, to provide
  // an an alternative dispatch target for the test only.  The callback
  // parameter is owned and deleted by this object.
  explicit PrerenderInterceptor(PrerenderInterceptorCallback* callback);

  void RunCallbackFromUIThread(const GURL& url);
  void PrerenderDispatch(const GURL& url);

  scoped_ptr<PrerenderInterceptorCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderInterceptor);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_PRERENDER_INTERCEPTOR_H_

