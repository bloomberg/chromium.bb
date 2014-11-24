// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_STORE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_STORE_H_

#include <deque>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
class TimeDelta;
class Value;
}

namespace net {
class BoundNetLog;
class NetLog;
}

namespace data_reduction_proxy {

class DataReductionProxyEventStore {
 public:
  // Constructs a DataReductionProxyEventStore object with the given network
  // task runner.
  explicit DataReductionProxyEventStore(
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner);

  ~DataReductionProxyEventStore();

  // Adds the DATA_REDUCTION_PROXY_ENABLED event (with enabled=true) to the
  // event store.
  void AddProxyEnabledEvent(
      net::NetLog* net_log,
      bool primary_restricted,
      bool fallback_restricted,
      const std::string& primary_origin,
      const std::string& fallback_origin,
      const std::string& ssl_origin);

  // Adds the DATA_REDUCTION_PROXY_ENABLED event (with enabled=false) to the
  // event store.
  void AddProxyDisabledEvent(net::NetLog* net_log);

  // Adds a DATA_REDUCTION_PROXY_BYPASS_REQUESTED event to the event store
  // when the bypass reason is initiated by the data reduction proxy.
  void AddBypassActionEvent(
      const net::BoundNetLog& net_log,
      const std::string& bypass_action,
      const GURL& gurl,
      const base::TimeDelta& bypass_duration);

  // Adds a DATA_REDUCTION_PROXY_BYPASS_REQUESTED event to the event store
  // when the bypass reason is not initiated by the data reduction proxy, such
  // as network errors.
  void AddBypassTypeEvent(
      const net::BoundNetLog& net_log,
      DataReductionProxyBypassType bypass_type,
      const GURL& gurl,
      const base::TimeDelta& bypass_duration);

  // Adds a DATA_REDUCTION_PROXY_CANARY_REQUEST event to the event store
  // when the canary request has started.
  void BeginCanaryRequest(const net::BoundNetLog& net_log, const GURL& gurl);

  // Adds a DATA_REDUCTION_PROXY_CANARY_REQUEST event to the event store
  // when the canary request has ended.
  void EndCanaryRequest(const net::BoundNetLog& net_log, int net_error);

 private:
  friend class DataReductionProxyEventStoreTest;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyEventStoreTest,
                           TestAddProxyEnabledEvent);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyEventStoreTest,
                           TestAddProxyDisabledEvent);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyEventStoreTest,
                           TestAddBypassActionEvent);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyEventStoreTest,
                           TestAddBypassTypeEvent);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyEventStoreTest,
                           TestBeginCanaryRequest);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyEventStoreTest,
                           TestEndCanaryRequest);

  // Prepare and post an event for the event_store on the global net_log.
  void PostGlobalNetLogEvent(net::NetLog* net_log,
                             net::NetLog::EventType type,
                             const net::NetLog::ParametersCallback& callback);

  // Prepare and post an event for the event_store on a BoundNetLog.
  void PostBoundNetLogEvent(const net::BoundNetLog& net_log,
                            net::NetLog::EventType type,
                            net::NetLog::EventPhase phase,
                            const net::NetLog::ParametersCallback& callback);

  // Put |entry| on a deque of events to store
  void AddEventOnIOThread(scoped_ptr<base::Value> entry);

  // A task runner to ensure that all reads/writes to |stored_events_| takes
  // place on the IO thread.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  // A deque of data reduction proxy related events. It is used as a circular
  // buffer to prevent unbounded memory utilization.
  std::deque<base::Value*> stored_events_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyEventStore);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_STORE_H_
