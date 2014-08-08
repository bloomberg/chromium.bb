// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_
#define COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/clear_mode.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/context.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "components/domain_reliability/scheduler.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_status.h"

namespace base {
class SingleThreadTaskRunner;
class ThreadChecker;
class Value;
}  // namespace base

namespace net {
class URLRequest;
class URLRequestContext;
class URLRequestContextGetter;
}  // namespace net

namespace domain_reliability {

// The top-level object that measures requests and hands off the measurements
// to the proper |DomainReliabilityContext|.
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityMonitor {
 public:
  explicit DomainReliabilityMonitor(const std::string& upload_reporter_string);
  DomainReliabilityMonitor(const std::string& upload_reporter_string,
                           scoped_ptr<MockableTime> time);
  ~DomainReliabilityMonitor();

  // Initializes the Monitor.
  void Init(
      net::URLRequestContext* url_request_context,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Same, but for unittests where the Getter is readily available.
  void Init(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

  // Populates the monitor with contexts that were configured at compile time.
  void AddBakedInConfigs();

  // Should be called when |request| is about to follow a redirect. Will
  // examine and possibly log the redirect request.
  void OnBeforeRedirect(net::URLRequest* request);

  // Should be called when |request| is complete. Will examine and possibly
  // log the (final) request. (|started| should be true if the request was
  // actually started before it was terminated.)
  void OnCompleted(net::URLRequest* request, bool started);

  // Called to remove browsing data. With CLEAR_BEACONS, leaves contexts in
  // place but clears beacons (which betray browsing history); with
  // CLEAR_CONTEXTS, removes all contexts (which can behave as cookies).
  void ClearBrowsingData(DomainReliabilityClearMode mode);

  // Gets a Value containing data that can be formatted into a web page for
  // debugging purposes.
  scoped_ptr<base::Value> GetWebUIData() const;

  DomainReliabilityContext* AddContextForTesting(
      scoped_ptr<const DomainReliabilityConfig> config);

  size_t contexts_size_for_testing() const { return contexts_.size(); }

 private:
  friend class DomainReliabilityMonitorTest;
  // Allow the Service to call |MakeWeakPtr|.
  friend class DomainReliabilityServiceImpl;

  typedef std::map<std::string, DomainReliabilityContext*> ContextMap;

  struct DOMAIN_RELIABILITY_EXPORT RequestInfo {
    RequestInfo();
    explicit RequestInfo(const net::URLRequest& request);
    ~RequestInfo();

    bool AccessedNetwork() const;

    GURL url;
    net::URLRequestStatus status;
    net::HttpResponseInfo response_info;
    int load_flags;
    net::LoadTimingInfo load_timing_info;
    bool is_upload;
  };

  // Creates a context, adds it to the monitor, and returns a pointer to it.
  // (The pointer is only valid until the Monitor is destroyed.)
  DomainReliabilityContext* AddContext(
      scoped_ptr<const DomainReliabilityConfig> config);
  // Deletes all contexts from |contexts_| and clears the map.
  void ClearContexts();
  void OnRequestLegComplete(const RequestInfo& info);

  DomainReliabilityContext* GetContextForHost(const std::string& host) const;

  base::WeakPtr<DomainReliabilityMonitor> MakeWeakPtr();

  scoped_ptr<base::ThreadChecker> thread_checker_;
  scoped_ptr<MockableTime> time_;
  const std::string upload_reporter_string_;
  DomainReliabilityScheduler::Params scheduler_params_;
  DomainReliabilityDispatcher dispatcher_;
  scoped_ptr<DomainReliabilityUploader> uploader_;
  ContextMap contexts_;

  base::WeakPtrFactory<DomainReliabilityMonitor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityMonitor);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_
