// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_session.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sample_vector.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_socket_pool.h"
#include "net/socket/stream_socket.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

namespace {
// Never exceed max timeout.
const unsigned kMaxTimeoutMs = 5000;
// Set min timeout, in case we are talking to a local DNS proxy.
const unsigned kMinTimeoutMs = 10;

// Number of buckets in the histogram of observed RTTs.
const size_t kRTTBucketCount = 100;
// Target percentile in the RTT histogram used for retransmission timeout.
const unsigned kRTOPercentile = 99;
}  // namespace

DnsSession::SocketLease::SocketLease(scoped_refptr<DnsSession> session,
                                     unsigned server_index,
                                     scoped_ptr<DatagramClientSocket> socket)
    : session_(session), server_index_(server_index), socket_(socket.Pass()) {}

DnsSession::SocketLease::~SocketLease() {
  session_->FreeSocket(server_index_, socket_.Pass());
}

DnsSession::DnsSession(const DnsConfig& config,
                       scoped_ptr<DnsSocketPool> socket_pool,
                       const RandIntCallback& rand_int_callback,
                       NetLog* net_log)
    : config_(config),
      socket_pool_(socket_pool.Pass()),
      rand_callback_(base::Bind(rand_int_callback, 0, kuint16max)),
      net_log_(net_log),
      server_index_(0),
      rtt_estimates_(config_.nameservers.size(), config_.timeout),
      rtt_deviations_(config_.nameservers.size()),
      rtt_buckets_(new base::BucketRanges(kRTTBucketCount + 1)) {
  socket_pool_->Initialize(&config_.nameservers, net_log);

  // TODO(mef): This could be done once per process lifetime.
  base::Histogram::InitializeBucketRanges(1, 5000, kRTTBucketCount,
                                          rtt_buckets_.get());
  for (size_t i = 0; i < config_.nameservers.size(); ++i) {
    rtt_histograms_.push_back(new base::SampleVector(rtt_buckets_.get()));
  }
}

DnsSession::~DnsSession() {}

int DnsSession::NextQueryId() const { return rand_callback_.Run(); }

int DnsSession::NextFirstServerIndex() {
  int index = server_index_;
  if (config_.rotate)
    server_index_ = (server_index_ + 1) % config_.nameservers.size();
  return index;
}

void DnsSession::RecordRTT(unsigned server_index, base::TimeDelta rtt) {
  DCHECK_LT(server_index, rtt_histograms_.size());

  // For measurement, assume it is the first attempt (no backoff).
  base::TimeDelta timeout_jacobson = NextTimeoutFromJacobson(server_index, 0);
  base::TimeDelta timeout_histogram = NextTimeoutFromHistogram(server_index, 0);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorJacobson", rtt - timeout_jacobson);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorHistogram",
                      rtt - timeout_histogram);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorJacobsonUnder",
                      timeout_jacobson - rtt);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorHistogramUnder",
                      timeout_histogram - rtt);

  // Jacobson/Karels algorithm for TCP.
  // Using parameters: alpha = 1/8, delta = 1/4, beta = 4
  base::TimeDelta& estimate = rtt_estimates_[server_index];
  base::TimeDelta& deviation = rtt_deviations_[server_index];
  base::TimeDelta current_error = rtt - estimate;
  estimate += current_error / 8;  // * alpha
  base::TimeDelta abs_error = base::TimeDelta::FromInternalValue(
      std::abs(current_error.ToInternalValue()));
  deviation += (abs_error - deviation) / 4;  // * delta

  // Histogram-based method.
  rtt_histograms_[server_index]->Accumulate(rtt.InMilliseconds(), 1);
}

void DnsSession::RecordLostPacket(unsigned server_index, int attempt) {
  base::TimeDelta timeout_jacobson =
      NextTimeoutFromJacobson(server_index, attempt);
  base::TimeDelta timeout_histogram =
      NextTimeoutFromHistogram(server_index, attempt);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutSpentJacobson", timeout_jacobson);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutSpentHistogram", timeout_histogram);
}

base::TimeDelta DnsSession::NextTimeout(unsigned server_index, int attempt) {
  DCHECK_LT(server_index, rtt_histograms_.size());

  base::TimeDelta timeout = config_.timeout;

  // The timeout doubles every full round (each nameserver once).
  unsigned num_backoffs = attempt / config_.nameservers.size();

  return std::min(timeout * (1 << num_backoffs),
                  base::TimeDelta::FromMilliseconds(kMaxTimeoutMs));
}

// Allocate a socket, already connected to the server address.
scoped_ptr<DnsSession::SocketLease> DnsSession::AllocateSocket(
    unsigned server_index, const NetLog::Source& source) {
  scoped_ptr<DatagramClientSocket> socket;

  socket = socket_pool_->AllocateSocket(server_index);
  if (!socket.get()) return scoped_ptr<SocketLease>(NULL);

  socket->NetLog().BeginEvent(NetLog::TYPE_SOCKET_IN_USE,
                              source.ToEventParametersCallback());

  SocketLease* lease = new SocketLease(this, server_index, socket.Pass());
  return scoped_ptr<SocketLease>(lease);
}

scoped_ptr<StreamSocket> DnsSession::CreateTCPSocket(
    unsigned server_index, const NetLog::Source& source) {
  return socket_pool_->CreateTCPSocket(server_index, source);
}

// Release a socket.
void DnsSession::FreeSocket(unsigned server_index,
                            scoped_ptr<DatagramClientSocket> socket) {
  DCHECK(socket.get());

  socket->NetLog().EndEvent(NetLog::TYPE_SOCKET_IN_USE);

  socket_pool_->FreeSocket(server_index, socket.Pass());
}

base::TimeDelta DnsSession::NextTimeoutFromJacobson(unsigned server_index,
                                                    int attempt) {
  DCHECK_LT(server_index, rtt_estimates_.size());

  base::TimeDelta timeout =
      rtt_estimates_[server_index] + 4 * rtt_deviations_[server_index];

  timeout = std::max(timeout, base::TimeDelta::FromMilliseconds(kMinTimeoutMs));

  // The timeout doubles every full round.
  unsigned num_backoffs = attempt / config_.nameservers.size();

  return std::min(timeout * (1 << num_backoffs),
                  base::TimeDelta::FromMilliseconds(kMaxTimeoutMs));
}

base::TimeDelta DnsSession::NextTimeoutFromHistogram(unsigned server_index,
                                                     int attempt) {
  DCHECK_LT(server_index, rtt_histograms_.size());

  COMPILE_ASSERT(std::numeric_limits<base::HistogramBase::Count>::is_signed,
                 histogram_base_count_assumed_to_be_signed);

  // Use fixed percentile of observed samples.
  const base::SampleVector& samples = *rtt_histograms_[server_index];
  base::HistogramBase::Count total = samples.TotalCount();
  base::HistogramBase::Count remaining_count = kRTOPercentile * total / 100;
  size_t index = 0;
  while (remaining_count > 0 && index < rtt_buckets_->size()) {
    remaining_count -= samples.GetCountAtIndex(index);
    ++index;
  }

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(rtt_buckets_->range(index));

  timeout = std::max(timeout, base::TimeDelta::FromMilliseconds(kMinTimeoutMs));

  // The timeout still doubles every full round.
  unsigned num_backoffs = attempt / config_.nameservers.size();

  return std::min(timeout * (1 << num_backoffs),
                  base::TimeDelta::FromMilliseconds(kMaxTimeoutMs));
}

}  // namespace net
