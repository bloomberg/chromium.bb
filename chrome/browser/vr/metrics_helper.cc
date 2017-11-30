// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/metrics_helper.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "net/base/network_change_notifier.h"

namespace vr {

namespace {

constexpr char kStatusVr[] = "VR.AssetsComponent.Status.OnEnter.VR";
constexpr char kStatusVrBrowsing[] =
    "VR.AssetsComponent.Status.OnEnter.VRBrowsing";
constexpr char kStatusWebVr[] = "VR.AssetsComponent.Status.OnEnter.WebVR";
constexpr char kLatencyVrBrowsing[] =
    "VR.AssetsComponent.ReadyLatency.OnEnter.VRBrowsing";
constexpr char kLatencyWebVr[] =
    "VR.AssetsComponent.ReadyLatency.OnEnter.WebVR";
constexpr char kDataConnectionRegisterComponent[] =
    "VR.DataConnection.OnRegisterAssetsComponent";
constexpr char kDataConnectionVr[] = "VR.DataConnection.OnEnter.VR";
constexpr char kDataConnectionVrBrowsing[] =
    "VR.DataConnection.OnEnter.VRBrowsing";
constexpr char kDataConnectionWebVr[] = "VR.DataConnection.OnEnter.WebVR";

const auto kMinLatency = base::TimeDelta::FromMilliseconds(500);
const auto kMaxLatency = base::TimeDelta::FromHours(1);
constexpr size_t kLatencyBucketCount = 100;

// Ensure that this stays in sync with VRAssetsComponentEnterStatus in
// enums.xml.
enum class AssetsComponentEnterStatus : int {
  kReady = 0,
  kUnreadyOther = 1,

  // This must be last.
  kCount,
};

void LogStatus(Mode mode, AssetsComponentEnterStatus status) {
  switch (mode) {
    case Mode::kVr:
      UMA_HISTOGRAM_ENUMERATION(kStatusVr, status,
                                AssetsComponentEnterStatus::kCount);
      return;
    case Mode::kVrBrowsing:
      UMA_HISTOGRAM_ENUMERATION(kStatusVrBrowsing, status,
                                AssetsComponentEnterStatus::kCount);
      return;
    case Mode::kWebVr:
      UMA_HISTOGRAM_ENUMERATION(kStatusWebVr, status,
                                AssetsComponentEnterStatus::kCount);
      return;
    default:
      NOTIMPLEMENTED();
      return;
  }
}

void LogLatency(Mode mode, const base::TimeDelta& latency) {
  switch (mode) {
    case Mode::kVrBrowsing:
      UMA_HISTOGRAM_CUSTOM_TIMES(kLatencyVrBrowsing, latency, kMinLatency,
                                 kMaxLatency, kLatencyBucketCount);
      return;
    case Mode::kWebVr:
      UMA_HISTOGRAM_CUSTOM_TIMES(kLatencyWebVr, latency, kMinLatency,
                                 kMaxLatency, kLatencyBucketCount);
      return;
    default:
      NOTIMPLEMENTED();
      return;
  }
}

void LogConnectionType(Mode mode,
                       net::NetworkChangeNotifier::ConnectionType type) {
  switch (mode) {
    case Mode::kVr:
      UMA_HISTOGRAM_ENUMERATION(
          kDataConnectionVr, type,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
      return;
    case Mode::kVrBrowsing:
      UMA_HISTOGRAM_ENUMERATION(
          kDataConnectionVrBrowsing, type,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
      return;
    case Mode::kWebVr:
      UMA_HISTOGRAM_ENUMERATION(
          kDataConnectionWebVr, type,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
      return;
    default:
      NOTIMPLEMENTED();
      return;
  }
}

}  // namespace

MetricsHelper::MetricsHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MetricsHelper::~MetricsHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MetricsHelper::OnComponentReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  component_ready_ = true;
  auto now = base::Time::Now();
  LogLatencyIfWaited(Mode::kVrBrowsing, now);
  LogLatencyIfWaited(Mode::kWebVr, now);
}

void MetricsHelper::OnEnter(Mode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogConnectionType(mode, net::NetworkChangeNotifier::GetConnectionType());
  auto& enter_time = GetEnterTime(mode);
  if (enter_time) {
    // While we are stopping the time between entering and component readiness
    // we pretend the user is waiting on the block screen. Do not report further
    // UMA metrics.
    return;
  }
  LogStatus(mode, component_ready_ ? AssetsComponentEnterStatus::kReady
                                   : AssetsComponentEnterStatus::kUnreadyOther);
  if (!component_ready_) {
    enter_time = base::Time::Now();
  }
}

void MetricsHelper::OnRegisteredComponent() {
  UMA_HISTOGRAM_ENUMERATION(
      kDataConnectionRegisterComponent,
      net::NetworkChangeNotifier::GetConnectionType(),
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
}

base::Optional<base::Time>& MetricsHelper::GetEnterTime(Mode mode) {
  switch (mode) {
    case Mode::kVr:
      return enter_vr_time_;
    case Mode::kVrBrowsing:
      return enter_vr_browsing_time_;
    case Mode::kWebVr:
      return enter_web_vr_time_;
    default:
      NOTIMPLEMENTED();
      return enter_vr_time_;
  }
}

void MetricsHelper::LogLatencyIfWaited(Mode mode, const base::Time& now) {
  auto& enter_time = GetEnterTime(mode);
  if (enter_time) {
    LogLatency(mode, now - *enter_time);
    enter_time = base::nullopt;
  }
}

}  // namespace vr
