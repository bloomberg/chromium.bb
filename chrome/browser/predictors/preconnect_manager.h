// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PRECONNECT_MANAGER_H_
#define CHROME_BROWSER_PREDICTORS_PRECONNECT_MANAGER_H_

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/http/http_request_info.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace predictors {

// Stores the status of all preconnects associated with a given |url|.
struct PreresolveInfo {
  PreresolveInfo(const GURL& url, size_t count);
  PreresolveInfo(const PreresolveInfo& other);
  ~PreresolveInfo();

  bool is_done() const { return queued_count == 0 && inflight_count == 0; }

  GURL url;
  size_t queued_count;
  size_t inflight_count = 0;
  bool was_canceled = false;
};

// Stores all data need for running a preresolve and a subsequent optional
// preconnect for a |url|.
struct PreresolveJob {
  PreresolveJob(const GURL& url, bool need_preconnect, PreresolveInfo* info);
  PreresolveJob(const PreresolveJob& other);
  ~PreresolveJob();

  GURL url;
  bool need_preconnect;
  // Raw pointer usage is fine here because even though PreresolveJob can
  // outlive PreresolveInfo it's only accessed on PreconnectManager class
  // context and PreresolveInfo lifetime is tied to PreconnectManager.
  PreresolveInfo* info;
};

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

  static const size_t kMaxInflightPreresolves = 3;

  PreconnectManager(base::WeakPtr<Delegate> delegate,
                    scoped_refptr<net::URLRequestContextGetter> context_getter);
  virtual ~PreconnectManager();

  // Starts preconnect and preresolve jobs keyed by |url|.
  void Start(const GURL& url,
             const std::vector<GURL>& preconnect_origins,
             const std::vector<GURL>& preresolve_hosts);
  // No additional jobs keyed by the |url| will be queued after this.
  void Stop(const GURL& url);

  // Public for mocking in unit tests. Don't use, internal only.
  virtual void PreconnectUrl(const GURL& url,
                             const GURL& first_party_for_cookies) const;
  virtual int PreresolveUrl(const GURL& url,
                            const net::CompletionCallback& callback) const;

 private:
  void TryToLaunchPreresolveJobs();
  void OnPreresolveFinished(const PreresolveJob& job, int result);
  void FinishPreresolve(const PreresolveJob& job, bool found);
  void AllPreresolvesForUrlFinished(PreresolveInfo* info);

  base::WeakPtr<Delegate> delegate_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  std::list<PreresolveJob> queued_jobs_;
  std::map<GURL, std::unique_ptr<PreresolveInfo>> preresolve_info_;
  size_t inflight_preresolves_count_ = 0;

  base::WeakPtrFactory<PreconnectManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PreconnectManager);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PRECONNECT_MANAGER_H_
