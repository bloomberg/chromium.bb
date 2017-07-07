// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PRECONNECT_MANAGER_H_
#define CHROME_BROWSER_PREDICTORS_PRECONNECT_MANAGER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace predictors {

// PreconnectManager is responsible for preresolving and preconnecting to
// origins based on the input list of URLs.
//  - The input list of URLs is associated with a main frame url that can be
//  used for cancelling.
//  - Limits the total number of preresolves in flight.
//  - Preresolves an URL before preconnecting to it to have a better control on
//  number of speculative dns requests in flight.
//  - When stopped, waits for the pending preresolve requests to finish without
//  issuing preconnects for them.
//  - All methods of the class except the constructor must be called on the IO
//  thread. The constructor must be called on the UI thread.
class PreconnectManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when all preresolve jobs for the |url| are finished. Note that
    // some preconnect jobs can be still in progress, because they are
    // fire-and-forget.
    // Is called on the UI thread.
    virtual void PreconnectFinished(const GURL& url) = 0;
  };

  PreconnectManager(base::WeakPtr<Delegate> delegate,
                    scoped_refptr<net::URLRequestContextGetter> context_getter);
  ~PreconnectManager();

  // Starts preconnect and preresolve jobs keyed by |url|.
  void Start(const GURL& url,
             const std::vector<GURL>& preconnect_origins,
             const std::vector<GURL>& preresolve_hosts);
  // No additional jobs keyed by the |url| will be queued after this.
  void Stop(const GURL& url);

 private:
  base::WeakPtr<Delegate> delegate_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PRECONNECT_MANAGER_H_
