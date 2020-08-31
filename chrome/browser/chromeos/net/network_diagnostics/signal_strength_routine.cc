// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/signal_strength_routine.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
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
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

void GetNetworkConfigService(
    mojo::PendingReceiver<CrosNetworkConfig> receiver) {
  chromeos::network_config::BindToInProcessInstance(std::move(receiver));
}

// Represents the point below which the NIC signal strength is so
// weak that it is problematic. The values are between 0 and 100 (see
// src/platform2/shill/doc/service-api.txt).
constexpr int kSignalStrengthThreshold = 30;

}  // namespace

SignalStrengthRoutine::SignalStrengthRoutine() {
  set_verdict(mojom::RoutineVerdict::kNotRun);
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

SignalStrengthRoutine::~SignalStrengthRoutine() = default;

bool SignalStrengthRoutine::CanRun() {
  DCHECK(remote_cros_network_config_);
  return true;
}

void SignalStrengthRoutine::RunTest(SignalStrengthRoutineCallback callback) {
  if (!CanRun()) {
    std::move(callback).Run(verdict(), std::move(problems_));
    return;
  }
  routine_completed_callback_ = std::move(callback);
  FetchActiveWirelessNetworks();
}

void SignalStrengthRoutine::AnalyzeResultsAndExecuteCallback() {
  if (signal_strength_ == kUnknownSignalStrength) {
    set_verdict(mojom::RoutineVerdict::kNotRun);
    problems_.emplace_back(mojom::SignalStrengthProblem::kSignalNotFound);
  } else if (signal_strength_ < kSignalStrengthThreshold) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::SignalStrengthProblem::kWeakSignal);
  } else {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  }
  std::move(routine_completed_callback_).Run(verdict(), std::move(problems_));
}

void SignalStrengthRoutine::FetchActiveWirelessNetworks() {
  DCHECK(remote_cros_network_config_);
  // The usage of `base::Unretained(this)` here is safe because
  // |remote_cros_network_config_| is a mojo::Remote owned by |this|.
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kWireless,
                         network_config::mojom::kNoLimit),
      base::BindOnce(&SignalStrengthRoutine::OnNetworkStateListReceived,
                     base::Unretained(this)));
}

// Process the network interface information.
void SignalStrengthRoutine::OnNetworkStateListReceived(
    std::vector<NetworkStatePropertiesPtr> networks) {
  for (const NetworkStatePropertiesPtr& network : networks) {
    if (network_config::StateIsConnected(network->connection_state)) {
      signal_strength_ =
          network_config::GetWirelessSignalStrength(network.get());
      break;
    }
  }
  AnalyzeResultsAndExecuteCallback();
}

}  // namespace network_diagnostics
}  // namespace chromeos
