// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_
#define COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/context.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "components/domain_reliability/scheduler.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_timing_info.h"
#include "net/url_request/url_request_status.h"

namespace net {
class URLRequest;
class URLRequestContext;
class URLRequestContextGetter;
}

namespace domain_reliability {

// The top-level per-profile object that measures requests and hands off the
// measurements to the proper |DomainReliabilityContext|.  Referenced by the
// |ChromeNetworkDelegate|, which calls the On* methods.
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityMonitor {
 public:
  // NB: We don't take a URLRequestContextGetter because we already live on the
  // I/O thread.
  explicit DomainReliabilityMonitor(
      net::URLRequestContext* url_request_context);
  DomainReliabilityMonitor(
      net::URLRequestContext* url_request_context,
      scoped_ptr<MockableTime> time);
  ~DomainReliabilityMonitor();

  // Adds the "baked-in" configuration(s) for Google sites.
  void AddBakedInConfigs();

  // Should be called from the profile's NetworkDelegate on the corresponding
  // events:
  void OnBeforeRedirect(net::URLRequest* request);
  void OnCompleted(net::URLRequest* request, bool started);

  DomainReliabilityContext* AddContextForTesting(
      scoped_ptr<const DomainReliabilityConfig> config);

  size_t contexts_size_for_testing() const { return contexts_.size(); }

 private:
  friend class DomainReliabilityMonitorTest;

  typedef std::map<std::string, DomainReliabilityContext*> ContextMap;

  struct DOMAIN_RELIABILITY_EXPORT RequestInfo {
    RequestInfo();
    RequestInfo(const net::URLRequest& request);
    ~RequestInfo();

    bool DefinitelyReachedNetwork() const;

    GURL url;
    net::URLRequestStatus status;
    int response_code;
    net::HostPortPair socket_address;
    net::LoadTimingInfo load_timing_info;
    bool was_cached;
    int load_flags;
    bool is_upload;
  };

  // Creates a context, adds it to the monitor, and returns a pointer to it.
  // (The pointer is only valid until the Monitor is destroyed.)
  DomainReliabilityContext* AddContext(
      scoped_ptr<const DomainReliabilityConfig> config);
  void OnRequestLegComplete(const RequestInfo& info);

  scoped_ptr<MockableTime> time_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  DomainReliabilityScheduler::Params scheduler_params_;
  DomainReliabilityDispatcher dispatcher_;
  scoped_ptr<DomainReliabilityUploader> uploader_;
  ContextMap contexts_;

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityMonitor);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_
