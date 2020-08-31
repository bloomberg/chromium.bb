// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/gateway_can_be_pinged_routine.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/values.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"

namespace chromeos {
namespace network_diagnostics {
namespace {

using chromeos::network_config::mojom::CrosNetworkConfig;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::ManagedPropertiesPtr;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

void GetNetworkConfigService(
    mojo::PendingReceiver<CrosNetworkConfig> receiver) {
  chromeos::network_config::BindToInProcessInstance(std::move(receiver));
}

// The maximum latency threshold (in milliseconds) for pinging the gateway.
constexpr base::TimeDelta kMaxAllowedLatencyMs =
    base::TimeDelta::FromMilliseconds(1500);

}  // namespace

GatewayCanBePingedRoutine::GatewayCanBePingedRoutine(
    chromeos::DebugDaemonClient* debug_daemon_client)
    : debug_daemon_client_(debug_daemon_client) {
  set_verdict(mojom::RoutineVerdict::kNotRun);
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

GatewayCanBePingedRoutine::~GatewayCanBePingedRoutine() = default;

bool GatewayCanBePingedRoutine::CanRun() {
  DCHECK(remote_cros_network_config_);
  return true;
}

void GatewayCanBePingedRoutine::RunTest(
    GatewayCanBePingedRoutineCallback callback) {
  if (!CanRun()) {
    std::move(callback).Run(verdict(), std::move(problems_));
    return;
  }
  routine_completed_callback_ = std::move(callback);
  FetchActiveNetworks();
}

void GatewayCanBePingedRoutine::AnalyzeResultsAndExecuteCallback() {
  if (unreachable_gateways_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::GatewayCanBePingedProblem::kUnreachableGateway);
  } else if (!pingable_default_network_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::GatewayCanBePingedProblem::kFailedToPingDefaultNetwork);
  } else if (default_network_latency_ > kMaxAllowedLatencyMs) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::GatewayCanBePingedProblem::kDefaultNetworkAboveLatencyThreshold);
  } else if (non_default_network_unsuccessful_ping_count_ > 0) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::GatewayCanBePingedProblem::kUnsuccessfulNonDefaultNetworksPings);
  } else if (!BelowLatencyThreshold()) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::GatewayCanBePingedProblem::
                               kNonDefaultNetworksAboveLatencyThreshold);
  } else {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  }
  std::move(routine_completed_callback_).Run(verdict(), std::move(problems_));
}

bool GatewayCanBePingedRoutine::BelowLatencyThreshold() {
  for (base::TimeDelta latency : non_default_network_latencies_) {
    if (latency > kMaxAllowedLatencyMs) {
      return false;
    }
  }
  return true;
}

void GatewayCanBePingedRoutine::FetchActiveNetworks() {
  DCHECK(remote_cros_network_config_);
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll,
                         network_config::mojom::kNoLimit),
      base::BindOnce(&GatewayCanBePingedRoutine::OnNetworkStateListReceived,
                     base::Unretained(this)));
}

void GatewayCanBePingedRoutine::FetchManagedProperties(
    const std::vector<std::string>& guids) {
  DCHECK(remote_cros_network_config_);
  guids_remaining_ = guids.size();
  for (const std::string& guid : guids) {
    remote_cros_network_config_->GetManagedProperties(
        guid,
        base::BindOnce(&GatewayCanBePingedRoutine::OnManagedPropertiesReceived,
                       base::Unretained(this)));
  }
}

void GatewayCanBePingedRoutine::PingGateways() {
  for (const std::string& gateway : gateways_) {
    debug_daemon_client()->TestICMP(
        gateway, base::BindOnce(&GatewayCanBePingedRoutine::OnTestICMPCompleted,
                                base::Unretained(this),
                                gateway == default_network_gateway_));
  }
}

// Parses |status| and returns the IP and latency. For details about |status|,
// please refer to:
// https://gerrit.chromium.org/gerrit/#/c/30310/2/src/helpers/icmp.cc.
bool GatewayCanBePingedRoutine::ParseICMPResult(const std::string& status,
                                                std::string* ip,
                                                base::TimeDelta* latency) {
  base::Optional<base::Value> parsed_value(base::JSONReader::Read(status));
  if (!parsed_value.has_value()) {
    return false;
  }
  if (!parsed_value->is_dict() || parsed_value->DictSize() != 1) {
    return false;
  }
  auto iter = parsed_value->DictItems().begin();
  const std::string& ip_addr = iter->first;
  const base::Value& info = iter->second;
  if (!info.is_dict()) {
    return false;
  }
  const base::Value* avg_value = info.FindKey("avg");
  if (!avg_value || !avg_value->is_double()) {
    return false;
  }
  *latency = base::TimeDelta::FromMilliseconds(avg_value->GetDouble());
  *ip = ip_addr;

  return true;
}

// Process the network interface information.
void GatewayCanBePingedRoutine::OnNetworkStateListReceived(
    std::vector<NetworkStatePropertiesPtr> networks) {
  bool connected = false;
  std::vector<std::string> guids;
  for (const auto& network : networks) {
    if (!network_config::StateIsConnected(network->connection_state)) {
      continue;
    }
    connected = true;
    const std::string& guid = network->guid;
    if (default_network_guid_.empty()) {
      default_network_guid_ = guid;
    }
    guids.emplace_back(guid);
  }
  if (!connected || guids.empty()) {
    // Since we are not connected at all, directly analyze the results.
    AnalyzeResultsAndExecuteCallback();
  } else {
    FetchManagedProperties(guids);
  }
}

void GatewayCanBePingedRoutine::OnManagedPropertiesReceived(
    ManagedPropertiesPtr managed_properties) {
  DCHECK(guids_remaining_ > 0);
  if (managed_properties) {
    if (managed_properties->ip_configs.has_value() &&
        managed_properties->ip_configs->size() != 0) {
      for (const auto& ip_config : managed_properties->ip_configs.value()) {
        if (ip_config->gateway.has_value()) {
          const std::string& gateway = ip_config->gateway.value();
          if (managed_properties->guid == default_network_guid_) {
            default_network_gateway_ = gateway;
          }
          gateways_.emplace_back(gateway);
        }
      }
    }
  }
  guids_remaining_--;
  if (guids_remaining_ == 0) {
    if (gateways_.size() == 0) {
      // Since we cannot ping the gateway, directly analyze the results.
      AnalyzeResultsAndExecuteCallback();
    } else {
      unreachable_gateways_ = false;
      gateways_remaining_ = gateways_.size();
      PingGateways();
    }
  }
}

void GatewayCanBePingedRoutine::OnTestICMPCompleted(
    bool is_default_network_ping_result,
    const base::Optional<std::string> status) {
  DCHECK(gateways_remaining_ > 0);
  std::string result_ip;
  base::TimeDelta result_latency;
  bool failed_ping =
      !status.has_value() ||
      !ParseICMPResult(status.value(), &result_ip, &result_latency);
  if (failed_ping) {
    if (!is_default_network_ping_result) {
      non_default_network_unsuccessful_ping_count_++;
    }
  } else {
    if (is_default_network_ping_result) {
      pingable_default_network_ = true;
      default_network_latency_ = result_latency;
    } else {
      non_default_network_latencies_.emplace_back(result_latency);
    }
  }
  gateways_remaining_--;
  if (gateways_remaining_ == 0) {
    AnalyzeResultsAndExecuteCallback();
  }
}

}  // namespace network_diagnostics
}  // namespace chromeos
