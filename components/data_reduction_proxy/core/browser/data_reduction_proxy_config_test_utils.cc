// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"

#include <stddef.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace net {
class NetworkQualityEstimator;
}

namespace data_reduction_proxy {

TestDataReductionProxyConfig::TestDataReductionProxyConfig(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    net::NetLog* net_log,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : TestDataReductionProxyConfig(
          base::MakeUnique<TestDataReductionProxyParams>(),
          io_task_runner,
          net_log,
          configurator,
          event_creator) {}

TestDataReductionProxyConfig::TestDataReductionProxyConfig(
    std::unique_ptr<DataReductionProxyConfigValues> config_values,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    net::NetLog* net_log,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : DataReductionProxyConfig(io_task_runner,
                               net_log,
                               std::move(config_values),
                               configurator,
                               event_creator),
      tick_clock_(nullptr),
      network_quality_prohibitively_slow_set_(false),
      network_quality_prohibitively_slow_(false),
      lofi_accuracy_recording_intervals_set_(false),
      is_captive_portal_(false) {
}

TestDataReductionProxyConfig::~TestDataReductionProxyConfig() {
}

bool TestDataReductionProxyConfig::IsNetworkQualityProhibitivelySlow(
    const net::NetworkQualityEstimator* network_quality_estimator) {
  if (network_quality_prohibitively_slow_set_)
    return network_quality_prohibitively_slow_;
  return DataReductionProxyConfig::IsNetworkQualityProhibitivelySlow(
      network_quality_estimator);
}

void TestDataReductionProxyConfig::ResetParamFlagsForTest() {
  config_values_ = base::MakeUnique<TestDataReductionProxyParams>();
}

TestDataReductionProxyParams* TestDataReductionProxyConfig::test_params() {
  return static_cast<TestDataReductionProxyParams*>(config_values_.get());
}

DataReductionProxyConfigValues* TestDataReductionProxyConfig::config_values() {
  return config_values_.get();
}

void TestDataReductionProxyConfig::ResetLoFiStatusForTest() {
  lofi_off_ = false;
}

void TestDataReductionProxyConfig::SetNetworkProhibitivelySlow(
    bool network_quality_prohibitively_slow) {
  network_quality_prohibitively_slow_set_ = true;
  network_quality_prohibitively_slow_ = network_quality_prohibitively_slow;
}

void TestDataReductionProxyConfig::SetLofiAccuracyRecordingIntervals(
    const std::vector<base::TimeDelta>& lofi_accuracy_recording_intervals) {
  lofi_accuracy_recording_intervals_set_ = true;
  lofi_accuracy_recording_intervals_ = lofi_accuracy_recording_intervals;
}

const std::vector<base::TimeDelta>&
TestDataReductionProxyConfig::GetLofiAccuracyRecordingIntervals() const {
  if (lofi_accuracy_recording_intervals_set_)
    return lofi_accuracy_recording_intervals_;
  return DataReductionProxyConfig::GetLofiAccuracyRecordingIntervals();
}

void TestDataReductionProxyConfig::SetTickClock(base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

base::TimeTicks TestDataReductionProxyConfig::GetTicksNow() const {
  if (tick_clock_)
    return tick_clock_->NowTicks();
  return DataReductionProxyConfig::GetTicksNow();
}

bool TestDataReductionProxyConfig::WasDataReductionProxyUsed(
    const net::URLRequest* request,
    DataReductionProxyTypeInfo* proxy_info) const {
  if (was_data_reduction_proxy_used_ &&
      !was_data_reduction_proxy_used_.value()) {
    return false;
  }
  bool was_data_reduction_proxy_used =
      DataReductionProxyConfig::WasDataReductionProxyUsed(request, proxy_info);
  if (proxy_info && was_data_reduction_proxy_used && proxy_index_)
    proxy_info->proxy_index = proxy_index_.value();
  return was_data_reduction_proxy_used;
}

void TestDataReductionProxyConfig::SetWasDataReductionProxyNotUsed() {
  was_data_reduction_proxy_used_ = false;
}

void TestDataReductionProxyConfig::SetWasDataReductionProxyUsedProxyIndex(
    int proxy_index) {
  proxy_index_ = proxy_index;
}

void TestDataReductionProxyConfig::ResetWasDataReductionProxyUsed() {
  was_data_reduction_proxy_used_.reset();
  proxy_index_.reset();
}

void TestDataReductionProxyConfig::SetIsCaptivePortal(bool is_captive_portal) {
  is_captive_portal_ = is_captive_portal;
}

bool TestDataReductionProxyConfig::GetIsCaptivePortal() const {
  return is_captive_portal_;
}

MockDataReductionProxyConfig::MockDataReductionProxyConfig(
    std::unique_ptr<DataReductionProxyConfigValues> config_values,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    net::NetLog* net_log,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : TestDataReductionProxyConfig(std::move(config_values),
                                   io_task_runner,
                                   net_log,
                                   configurator,
                                   event_creator) {}

MockDataReductionProxyConfig::~MockDataReductionProxyConfig() {
}

void MockDataReductionProxyConfig::ResetLoFiStatusForTest() {
  lofi_off_ = false;
}

}  // namespace data_reduction_proxy
