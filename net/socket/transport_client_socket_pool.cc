// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_pool.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/socket_net_log_params.h"
#include "net/socket/tcp_client_socket.h"

using base::TimeDelta;

namespace net {

// TODO(willchan): Base this off RTT instead of statically setting it. Note we
// choose a timeout that is different from the backup connect job timer so they
// don't synchronize.
const int TransportConnectJobHelper::kIPv6FallbackTimerInMs = 300;

namespace {

// Returns true iff all addresses in |list| are in the IPv6 family.
bool AddressListOnlyContainsIPv6(const AddressList& list) {
  DCHECK(!list.empty());
  for (AddressList::const_iterator iter = list.begin(); iter != list.end();
       ++iter) {
    if (iter->GetFamily() != ADDRESS_FAMILY_IPV6)
      return false;
  }
  return true;
}

}  // namespace

// This lock protects |g_last_connect_time|.
static base::LazyInstance<base::Lock>::Leaky
    g_last_connect_time_lock = LAZY_INSTANCE_INITIALIZER;

// |g_last_connect_time| has the last time a connect() call is made.
static base::LazyInstance<base::TimeTicks>::Leaky
    g_last_connect_time = LAZY_INSTANCE_INITIALIZER;

TransportSocketParams::TransportSocketParams(
    const HostPortPair& host_port_pair,
    bool disable_resolver_cache,
    bool ignore_limits,
    const OnHostResolutionCallback& host_resolution_callback)
    : destination_(host_port_pair),
      ignore_limits_(ignore_limits),
      host_resolution_callback_(host_resolution_callback) {
  if (disable_resolver_cache)
    destination_.set_allow_cached_response(false);
}

TransportSocketParams::~TransportSocketParams() {}

// TransportConnectJobs will time out after this many seconds.  Note this is
// the total time, including both host resolution and TCP connect() times.
//
// TODO(eroman): The use of this constant needs to be re-evaluated. The time
// needed for TCPClientSocketXXX::Connect() can be arbitrarily long, since
// the address list may contain many alternatives, and most of those may
// timeout. Even worse, the per-connect timeout threshold varies greatly
// between systems (anywhere from 20 seconds to 190 seconds).
// See comment #12 at http://crbug.com/23364 for specifics.
static const int kTransportConnectJobTimeoutInSeconds = 240;  // 4 minutes.

TransportConnectJobHelper::TransportConnectJobHelper(
    const scoped_refptr<TransportSocketParams>& params,
    ClientSocketFactory* client_socket_factory,
    HostResolver* host_resolver,
    LoadTimingInfo::ConnectTiming* connect_timing)
    : params_(params),
      client_socket_factory_(client_socket_factory),
      resolver_(host_resolver),
      next_state_(STATE_NONE),
      connect_timing_(connect_timing) {}

TransportConnectJobHelper::~TransportConnectJobHelper() {}

int TransportConnectJobHelper::DoResolveHost(RequestPriority priority,
                                             const BoundNetLog& net_log) {
  next_state_ = STATE_RESOLVE_HOST_COMPLETE;
  connect_timing_->dns_start = base::TimeTicks::Now();

  return resolver_.Resolve(
      params_->destination(), priority, &addresses_, on_io_complete_, net_log);
}

int TransportConnectJobHelper::DoResolveHostComplete(
    int result,
    const BoundNetLog& net_log) {
  connect_timing_->dns_end = base::TimeTicks::Now();
  // Overwrite connection start time, since for connections that do not go
  // through proxies, |connect_start| should not include dns lookup time.
  connect_timing_->connect_start = connect_timing_->dns_end;

  if (result == OK) {
    // Invoke callback, and abort if it fails.
    if (!params_->host_resolution_callback().is_null())
      result = params_->host_resolution_callback().Run(addresses_, net_log);

    if (result == OK)
      next_state_ = STATE_TRANSPORT_CONNECT;
  }
  return result;
}

base::TimeDelta TransportConnectJobHelper::HistogramDuration(
    ConnectionLatencyHistogram race_result) {
  DCHECK(!connect_timing_->connect_start.is_null());
  DCHECK(!connect_timing_->dns_start.is_null());
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta total_duration = now - connect_timing_->dns_start;
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.DNS_Resolution_And_TCP_Connection_Latency2",
                             total_duration,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(10),
                             100);

  base::TimeDelta connect_duration = now - connect_timing_->connect_start;
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency",
                             connect_duration,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(10),
                             100);

  switch (race_result) {
    case CONNECTION_LATENCY_IPV4_WINS_RACE:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv4_Wins_Race",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
      break;

    case CONNECTION_LATENCY_IPV4_NO_RACE:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv4_No_Race",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
      break;

    case CONNECTION_LATENCY_IPV6_RACEABLE:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv6_Raceable",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
      break;

    case CONNECTION_LATENCY_IPV6_SOLO:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv6_Solo",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
      break;

    default:
      NOTREACHED();
      break;
  }

  return connect_duration;
}

TransportConnectJob::TransportConnectJob(
    const std::string& group_name,
    RequestPriority priority,
    const scoped_refptr<TransportSocketParams>& params,
    base::TimeDelta timeout_duration,
    ClientSocketFactory* client_socket_factory,
    HostResolver* host_resolver,
    Delegate* delegate,
    NetLog* net_log)
    : ConnectJob(group_name, timeout_duration, priority, delegate,
                 BoundNetLog::Make(net_log, NetLog::SOURCE_CONNECT_JOB)),
      helper_(params, client_socket_factory, host_resolver, &connect_timing_),
      interval_between_connects_(CONNECT_INTERVAL_GT_20MS) {
  helper_.SetOnIOComplete(this);
}

TransportConnectJob::~TransportConnectJob() {
  // We don't worry about cancelling the host resolution and TCP connect, since
  // ~SingleRequestHostResolver and ~StreamSocket will take care of it.
}

LoadState TransportConnectJob::GetLoadState() const {
  switch (helper_.next_state()) {
    case TransportConnectJobHelper::STATE_RESOLVE_HOST:
    case TransportConnectJobHelper::STATE_RESOLVE_HOST_COMPLETE:
      return LOAD_STATE_RESOLVING_HOST;
    case TransportConnectJobHelper::STATE_TRANSPORT_CONNECT:
    case TransportConnectJobHelper::STATE_TRANSPORT_CONNECT_COMPLETE:
      return LOAD_STATE_CONNECTING;
    case TransportConnectJobHelper::STATE_NONE:
      return LOAD_STATE_IDLE;
  }
  NOTREACHED();
  return LOAD_STATE_IDLE;
}

// static
void TransportConnectJob::MakeAddressListStartWithIPv4(AddressList* list) {
  for (AddressList::iterator i = list->begin(); i != list->end(); ++i) {
    if (i->GetFamily() == ADDRESS_FAMILY_IPV4) {
      std::rotate(list->begin(), i, list->end());
      break;
    }
  }
}

int TransportConnectJob::DoResolveHost() {
  return helper_.DoResolveHost(priority(), net_log());
}

int TransportConnectJob::DoResolveHostComplete(int result) {
  return helper_.DoResolveHostComplete(result, net_log());
}

int TransportConnectJob::DoTransportConnect() {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks last_connect_time;
  {
    base::AutoLock lock(g_last_connect_time_lock.Get());
    last_connect_time = g_last_connect_time.Get();
    *g_last_connect_time.Pointer() = now;
  }
  if (last_connect_time.is_null()) {
    interval_between_connects_ = CONNECT_INTERVAL_GT_20MS;
  } else {
    int64 interval = (now - last_connect_time).InMilliseconds();
    if (interval <= 10)
      interval_between_connects_ = CONNECT_INTERVAL_LE_10MS;
    else if (interval <= 20)
      interval_between_connects_ = CONNECT_INTERVAL_LE_20MS;
    else
      interval_between_connects_ = CONNECT_INTERVAL_GT_20MS;
  }

  helper_.set_next_state(
      TransportConnectJobHelper::STATE_TRANSPORT_CONNECT_COMPLETE);
  transport_socket_ =
      helper_.client_socket_factory()->CreateTransportClientSocket(
          helper_.addresses(), net_log().net_log(), net_log().source());
  int rv = transport_socket_->Connect(helper_.on_io_complete());
  if (rv == ERR_IO_PENDING &&
      helper_.addresses().front().GetFamily() == ADDRESS_FAMILY_IPV6 &&
      !AddressListOnlyContainsIPv6(helper_.addresses())) {
    fallback_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(
            TransportConnectJobHelper::kIPv6FallbackTimerInMs),
        this,
        &TransportConnectJob::DoIPv6FallbackTransportConnect);
  }
  return rv;
}

int TransportConnectJob::DoTransportConnectComplete(int result) {
  if (result == OK) {
    bool is_ipv4 =
        helper_.addresses().front().GetFamily() == ADDRESS_FAMILY_IPV4;
    TransportConnectJobHelper::ConnectionLatencyHistogram race_result =
        TransportConnectJobHelper::CONNECTION_LATENCY_UNKNOWN;
    if (is_ipv4) {
      race_result = TransportConnectJobHelper::CONNECTION_LATENCY_IPV4_NO_RACE;
    } else {
      if (AddressListOnlyContainsIPv6(helper_.addresses())) {
        race_result = TransportConnectJobHelper::CONNECTION_LATENCY_IPV6_SOLO;
      } else {
        race_result =
            TransportConnectJobHelper::CONNECTION_LATENCY_IPV6_RACEABLE;
      }
    }
    base::TimeDelta connect_duration = helper_.HistogramDuration(race_result);
    switch (interval_between_connects_) {
      case CONNECT_INTERVAL_LE_10MS:
        UMA_HISTOGRAM_CUSTOM_TIMES(
            "Net.TCP_Connection_Latency_Interval_LessThanOrEqual_10ms",
            connect_duration,
            base::TimeDelta::FromMilliseconds(1),
            base::TimeDelta::FromMinutes(10),
            100);
        break;
      case CONNECT_INTERVAL_LE_20MS:
        UMA_HISTOGRAM_CUSTOM_TIMES(
            "Net.TCP_Connection_Latency_Interval_LessThanOrEqual_20ms",
            connect_duration,
            base::TimeDelta::FromMilliseconds(1),
            base::TimeDelta::FromMinutes(10),
            100);
        break;
      case CONNECT_INTERVAL_GT_20MS:
        UMA_HISTOGRAM_CUSTOM_TIMES(
            "Net.TCP_Connection_Latency_Interval_GreaterThan_20ms",
            connect_duration,
            base::TimeDelta::FromMilliseconds(1),
            base::TimeDelta::FromMinutes(10),
            100);
        break;
      default:
        NOTREACHED();
        break;
    }

    SetSocket(transport_socket_.Pass());
    fallback_timer_.Stop();
  } else {
    // Be a bit paranoid and kill off the fallback members to prevent reuse.
    fallback_transport_socket_.reset();
    fallback_addresses_.reset();
  }

  return result;
}

void TransportConnectJob::DoIPv6FallbackTransportConnect() {
  // The timer should only fire while we're waiting for the main connect to
  // succeed.
  if (helper_.next_state() !=
      TransportConnectJobHelper::STATE_TRANSPORT_CONNECT_COMPLETE) {
    NOTREACHED();
    return;
  }

  DCHECK(!fallback_transport_socket_.get());
  DCHECK(!fallback_addresses_.get());

  fallback_addresses_.reset(new AddressList(helper_.addresses()));
  MakeAddressListStartWithIPv4(fallback_addresses_.get());
  fallback_transport_socket_ =
      helper_.client_socket_factory()->CreateTransportClientSocket(
          *fallback_addresses_, net_log().net_log(), net_log().source());
  fallback_connect_start_time_ = base::TimeTicks::Now();
  int rv = fallback_transport_socket_->Connect(
      base::Bind(
          &TransportConnectJob::DoIPv6FallbackTransportConnectComplete,
          base::Unretained(this)));
  if (rv != ERR_IO_PENDING)
    DoIPv6FallbackTransportConnectComplete(rv);
}

void TransportConnectJob::DoIPv6FallbackTransportConnectComplete(int result) {
  // This should only happen when we're waiting for the main connect to succeed.
  if (helper_.next_state() !=
      TransportConnectJobHelper::STATE_TRANSPORT_CONNECT_COMPLETE) {
    NOTREACHED();
    return;
  }

  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(fallback_transport_socket_.get());
  DCHECK(fallback_addresses_.get());

  if (result == OK) {
    DCHECK(!fallback_connect_start_time_.is_null());
    connect_timing_.connect_start = fallback_connect_start_time_;
    helper_.HistogramDuration(
        TransportConnectJobHelper::CONNECTION_LATENCY_IPV4_WINS_RACE);
    SetSocket(fallback_transport_socket_.Pass());
    helper_.set_next_state(TransportConnectJobHelper::STATE_NONE);
    transport_socket_.reset();
  } else {
    // Be a bit paranoid and kill off the fallback members to prevent reuse.
    fallback_transport_socket_.reset();
    fallback_addresses_.reset();
  }
  NotifyDelegateOfCompletion(result);  // Deletes |this|
}

int TransportConnectJob::ConnectInternal() {
  return helper_.DoConnectInternal(this);
}

scoped_ptr<ConnectJob>
TransportClientSocketPool::TransportConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return scoped_ptr<ConnectJob>(
      new TransportConnectJob(group_name,
                              request.priority(),
                              request.params(),
                              ConnectionTimeout(),
                              client_socket_factory_,
                              host_resolver_,
                              delegate,
                              net_log_));
}

base::TimeDelta
    TransportClientSocketPool::TransportConnectJobFactory::ConnectionTimeout()
    const {
  return base::TimeDelta::FromSeconds(kTransportConnectJobTimeoutInSeconds);
}

TransportClientSocketPool::TransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    ClientSocketPoolHistograms* histograms,
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory,
    NetLog* net_log)
    : base_(NULL, max_sockets, max_sockets_per_group, histograms,
            ClientSocketPool::unused_idle_socket_timeout(),
            ClientSocketPool::used_idle_socket_timeout(),
            new TransportConnectJobFactory(client_socket_factory,
                                           host_resolver, net_log)) {
  base_.EnableConnectBackupJobs();
}

TransportClientSocketPool::~TransportClientSocketPool() {}

int TransportClientSocketPool::RequestSocket(
    const std::string& group_name,
    const void* params,
    RequestPriority priority,
    ClientSocketHandle* handle,
    const CompletionCallback& callback,
    const BoundNetLog& net_log) {
  const scoped_refptr<TransportSocketParams>* casted_params =
      static_cast<const scoped_refptr<TransportSocketParams>*>(params);

  NetLogTcpClientSocketPoolRequestedSocket(net_log, casted_params);

  return base_.RequestSocket(group_name, *casted_params, priority, handle,
                             callback, net_log);
}

void TransportClientSocketPool::NetLogTcpClientSocketPoolRequestedSocket(
    const BoundNetLog& net_log,
    const scoped_refptr<TransportSocketParams>* casted_params) {
  if (net_log.IsLogging()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(
        NetLog::TYPE_TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKET,
        CreateNetLogHostPortPairCallback(
            &casted_params->get()->destination().host_port_pair()));
  }
}

void TransportClientSocketPool::RequestSockets(
    const std::string& group_name,
    const void* params,
    int num_sockets,
    const BoundNetLog& net_log) {
  const scoped_refptr<TransportSocketParams>* casted_params =
      static_cast<const scoped_refptr<TransportSocketParams>*>(params);

  if (net_log.IsLogging()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(
        NetLog::TYPE_TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKETS,
        CreateNetLogHostPortPairCallback(
            &casted_params->get()->destination().host_port_pair()));
  }

  base_.RequestSockets(group_name, *casted_params, num_sockets, net_log);
}

void TransportClientSocketPool::CancelRequest(
    const std::string& group_name,
    ClientSocketHandle* handle) {
  base_.CancelRequest(group_name, handle);
}

void TransportClientSocketPool::ReleaseSocket(
    const std::string& group_name,
    scoped_ptr<StreamSocket> socket,
    int id) {
  base_.ReleaseSocket(group_name, socket.Pass(), id);
}

void TransportClientSocketPool::FlushWithError(int error) {
  base_.FlushWithError(error);
}

void TransportClientSocketPool::CloseIdleSockets() {
  base_.CloseIdleSockets();
}

int TransportClientSocketPool::IdleSocketCount() const {
  return base_.idle_socket_count();
}

int TransportClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_.IdleSocketCountInGroup(group_name);
}

LoadState TransportClientSocketPool::GetLoadState(
    const std::string& group_name, const ClientSocketHandle* handle) const {
  return base_.GetLoadState(group_name, handle);
}

base::DictionaryValue* TransportClientSocketPool::GetInfoAsValue(
    const std::string& name,
    const std::string& type,
    bool include_nested_pools) const {
  return base_.GetInfoAsValue(name, type);
}

base::TimeDelta TransportClientSocketPool::ConnectionTimeout() const {
  return base_.ConnectionTimeout();
}

ClientSocketPoolHistograms* TransportClientSocketPool::histograms() const {
  return base_.histograms();
}

bool TransportClientSocketPool::IsStalled() const {
  return base_.IsStalled();
}

void TransportClientSocketPool::AddHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.AddHigherLayeredPool(higher_pool);
}

void TransportClientSocketPool::RemoveHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.RemoveHigherLayeredPool(higher_pool);
}

}  // namespace net
