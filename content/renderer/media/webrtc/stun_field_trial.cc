// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/stun_field_trial.h"

#include <math.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "third_party/webrtc/base/asyncpacketsocket.h"
#include "third_party/webrtc/base/asyncresolverinterface.h"
#include "third_party/webrtc/base/ipaddress.h"
#include "third_party/webrtc/base/network.h"
#include "third_party/webrtc/base/socketaddress.h"
#include "third_party/webrtc/base/thread.h"
#include "third_party/webrtc/p2p/base/packetsocketfactory.h"
#include "third_party/webrtc/p2p/stunprober/stunprober.h"

using stunprober::StunProber;

namespace content {

namespace {

// This needs to be the same as NatTypeCounters in histograms.xml.
enum NatType {
  NAT_TYPE_NONE,
  NAT_TYPE_UNKNOWN,
  NAT_TYPE_SYMMETRIC,
  NAT_TYPE_NON_SYMMETRIC,
  NAT_TYPE_MAX
};

// This needs to match "NatType" in histograms.xml.
const char* NatTypeNames[] = {"NoNAT", "UnknownNAT", "SymNAT", "NonSymNAT"};
COMPILE_ASSERT(arraysize(NatTypeNames) == NAT_TYPE_MAX, NamesArraySizeNotMatch);

NatType GetNatType(stunprober::NatType nat_type) {
  switch (nat_type) {
    case stunprober::NATTYPE_NONE:
      return NAT_TYPE_NONE;
    case stunprober::NATTYPE_UNKNOWN:
      return NAT_TYPE_UNKNOWN;
    case stunprober::NATTYPE_SYMMETRIC:
      return NAT_TYPE_SYMMETRIC;
    case stunprober::NATTYPE_NON_SYMMETRIC:
      return NAT_TYPE_NON_SYMMETRIC;
    default:
      return NAT_TYPE_MAX;
  }
}

// Below 50ms, we are doing experiments at each 5ms interval. Beyond 50ms, only
// one experiment of 100ms.
int ClampProbingInterval(int interval_ms) {
  return interval_ms > 50 ? 100 : interval_ms;
}

std::string HistogramName(const std::string& prefix,
                          NatType nat_type,
                          int interval_ms) {
  return base::StringPrintf("WebRTC.Stun.%s.%s.%dms", prefix.c_str(),
                            NatTypeNames[nat_type], interval_ms);
}

void SaveHistogramData(StunProber* prober) {
  StunProber::Stats stats;
  if (!prober->GetStats(&stats))
    return;

  NatType nat_type = GetNatType(stats.nat_type);

  // Use the real probe interval for reporting, converting from nanosecond to
  // millisecond at 5ms boundary.
  int interval_ms =
      round(static_cast<float>(stats.actual_request_interval_ns) / 5000) * 5;

  interval_ms = ClampProbingInterval(interval_ms);

  UMA_HISTOGRAM_ENUMERATION("WebRTC.NAT.Metrics", nat_type, NAT_TYPE_MAX);

  std::string histogram_name =
      HistogramName("SuccessPercent", nat_type, interval_ms);

  // Mimic the same behavior as UMA_HISTOGRAM_PERCENTAGE. We can't use that
  // macro as the histogram name is determined dynamically.
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      histogram_name, 1, 101, 102, base::Histogram::kUmaTargetedHistogramFlag);
  histogram->Add(stats.success_percent);

  DVLOG(1) << "Histogram '" << histogram_name.c_str()
           << "' = " << stats.success_percent;

  histogram_name = HistogramName("ResponseLatency", nat_type, interval_ms);

  histogram = base::Histogram::FactoryTimeGet(
      histogram_name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(10), 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram->AddTime(base::TimeDelta::FromMilliseconds(stats.average_rtt_ms));

  DVLOG(1) << "Histogram '" << histogram_name.c_str()
           << "' = " << stats.average_rtt_ms << " ms";

  DVLOG(1) << "Shared Socket Mode: " << stats.shared_socket_mode;
  DVLOG(1) << "Requests sent: " << stats.num_request_sent;
  DVLOG(1) << "Responses received: " << stats.num_response_received;
  DVLOG(1) << "Target interval (ns): " << stats.target_request_interval_ns;
  DVLOG(1) << "Actual interval (ns): " << stats.actual_request_interval_ns;
  DVLOG(1) << "NAT Type: " << NatTypeNames[nat_type];
  DVLOG(1) << "Host IP: " << stats.host_ip;
  DVLOG(1) << "Server-reflexive ips: ";
  for (const auto& ip : stats.srflx_addrs)
    DVLOG(1) << "\t" << ip;
}

void OnStunProbeTrialFinished(StunProber* prober, int result) {
  if (result == StunProber::SUCCESS)
    SaveHistogramData(prober);
}

}  // namespace

bool ParseStunProbeParameters(const std::string& params,
                              int* requests_per_ip,
                              int* interval_ms,
                              int* shared_socket_mode,
                              std::vector<rtc::SocketAddress>* servers) {
  std::vector<std::string> stun_params;
  base::SplitString(params, '/', &stun_params);

  if (stun_params.size() < 4) {
    DLOG(ERROR) << "Not enough parameters specified in StartStunProbeTrial";
    return false;
  }
  auto param = stun_params.begin();

  if (param->empty()) {
    *requests_per_ip = 10;
  } else if (!base::StringToInt(*param, requests_per_ip)) {
    DLOG(ERROR) << "Failed to parse request_per_ip in StartStunProbeTrial";
    return false;
  }
  param++;

  // Set inter-probe interval randomly from 0, 5, 10, ... 50, 100 ms.
  if ((*param).empty()) {
    *interval_ms = base::RandInt(0, 11) * 5;
  } else if (!base::StringToInt(*param, interval_ms)) {
    DLOG(ERROR) << "Failed to parse interval in StartStunProbeTrial";
    return false;
  }
  *interval_ms = ClampProbingInterval(*interval_ms);
  param++;

  if ((*param).empty()) {
    *shared_socket_mode = base::RandInt(0, 1);
  } else if (!base::StringToInt(*param, shared_socket_mode)) {
    DLOG(ERROR) << "Failed to parse shared_socket_mode in StartStunProbeTrial";
    return false;
  }
  param++;

  while (param != stun_params.end() && !param->empty()) {
    rtc::SocketAddress server;
    if (!server.FromString(*param)) {
      DLOG(ERROR) << "Failed to parse address in StartStunProbeTrial";
      return false;
    }
    servers->push_back(server);
    param++;
  }

  return !servers->empty();
}

scoped_ptr<stunprober::StunProber> StartStunProbeTrial(
    const rtc::NetworkManager::NetworkList& networks,
    const std::string& params,
    rtc::PacketSocketFactory* factory) {
  DVLOG(1) << "Starting stun trial with params: " << params;

  // If we don't have local addresses, we won't be able to determine whether
  // we're behind NAT or not.
  if (networks.empty()) {
    DLOG(ERROR) << "No networks specified in StartStunProbeTrial";
    return nullptr;
  }

  int requests_per_ip;
  int interval_ms;
  int shared_socket_mode;
  std::vector<rtc::SocketAddress> servers;

  if (!ParseStunProbeParameters(params, &requests_per_ip, &interval_ms,
                                &shared_socket_mode, &servers)) {
    return nullptr;
  }

  scoped_ptr<StunProber> prober(
      new StunProber(factory, rtc::Thread::Current(), networks));

  if (!prober->Start(
          servers, (shared_socket_mode != 0), interval_ms, requests_per_ip,
          1000,
          rtc::Callback2<void, StunProber*, int>(&OnStunProbeTrialFinished))) {
    DLOG(ERROR) << "Failed to Start in StartStunProbeTrial";
    OnStunProbeTrialFinished(prober.get(), StunProber::GENERIC_FAILURE);
    return nullptr;
  }

  return prober;
}

}  // namespace content
