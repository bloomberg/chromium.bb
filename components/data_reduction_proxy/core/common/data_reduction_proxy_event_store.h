// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_STORE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_STORE_H_

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/base/net_log.h"

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

enum SecureProxyCheckState {
  CHECK_UNKNOWN,
  CHECK_PENDING,
  CHECK_SUCCESS,
  CHECK_FAILED,
};

class DataReductionProxyEventStore {
 public:
  // Adds data reduction proxy specific constants to the net_internals
  // constants dictionary.
  static void AddConstants(base::DictionaryValue* constants_dict);

  // Constructs a DataReductionProxyEventStore object with the given UI
  // task runner.
  explicit DataReductionProxyEventStore(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner);

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
  // when the secure proxy request has started.
  void BeginSecureProxyCheck(const net::BoundNetLog& net_log, const GURL& gurl);

  // Adds a DATA_REDUCTION_PROXY_CANARY_REQUEST event to the event store
  // when the secure proxy request has ended.
  void EndSecureProxyCheck(const net::BoundNetLog& net_log, int net_error);

  // Creates a Value summary of Data Reduction Proxy related information:
  // - Whether the proxy is enabled
  // - The proxy configuration
  // - The state of the last secure proxy check response
  // - A stream of the last Data Reduction Proxy related events.
  // The caller is responsible for deleting the returned value.
  base::Value* GetSummaryValue() const;

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
                           TestBeginSecureProxyCheck);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyEventStoreTest,
                           TestEndSecureProxyCheck);

  // Prepare and post enabling/disabling proxy events for the event_store on the
  // global net_log.
  void PostEnabledEvent(net::NetLog* net_log,
                        net::NetLog::EventType type,
                        bool enable,
                        const net::NetLog::ParametersCallback& callback);

  // Prepare and post a Data Reduction Proxy bypass event for the event_store
  // on a BoundNetLog.
  void PostBoundNetLogBypassEvent(
      const net::BoundNetLog& net_log,
      net::NetLog::EventType type,
      net::NetLog::EventPhase phase,
      int64 expiration_ticks,
      const net::NetLog::ParametersCallback& callback);

  // Prepare and post a secure proxy check event for the event_store on a
  // BoundNetLog.
  void PostBoundNetLogSecureProxyCheckEvent(
      const net::BoundNetLog& net_log,
      net::NetLog::EventType type,
      net::NetLog::EventPhase phase,
      SecureProxyCheckState state,
      const net::NetLog::ParametersCallback& callback);

  // Put |entry| on a deque of events to store
  void AddEventOnUIThread(scoped_ptr<base::Value> entry);

  // Put |entry| on the deque of stored events and set |current_configuration_|.
  void AddEnabledEventOnUIThread(scoped_ptr<base::Value> entry, bool enabled);

  // Put |entry| on a deque of events to store and set
  // |secure_proxy_check_state_|
  void AddEventAndSecureProxyCheckStateOnUIThread(scoped_ptr<base::Value> entry,
                                                  SecureProxyCheckState state);

  // Put |entry| on a deque of events to store and set |last_bypass_event_| and
  // |expiration_ticks_|
  void AddAndSetLastBypassEventOnUIThread(scoped_ptr<base::Value> entry,
                                          int64 expiration_ticks);

  // A task runner to ensure that all reads/writes to |stored_events_| takes
  // place on the UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  // A deque of data reduction proxy related events. It is used as a circular
  // buffer to prevent unbounded memory utilization.
  std::deque<base::Value*> stored_events_;
  // Whether the data reduction proxy is enabled or not.
  bool enabled_;
  // The current data reduction proxy configuration.
  scoped_ptr<base::Value> current_configuration_;
  // The state based on the last secure proxy check.
  SecureProxyCheckState secure_proxy_check_state_;
  // The last seen data reduction proxy bypass event.
  scoped_ptr<base::Value> last_bypass_event_;
  // The expiration time of the |last_bypass_event_|.
  int64 expiration_ticks_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyEventStore);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_STORE_H_
