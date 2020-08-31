// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_HAS_SECURE_WIFI_CONNECTION_ROUTINE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_HAS_SECURE_WIFI_CONNECTION_ROUTINE_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace network_diagnostics {

// Tests whether the WiFi connection uses a secure encryption method.
class HasSecureWiFiConnectionRoutine : public NetworkDiagnosticsRoutine {
 public:
  using HasSecureWiFiConnectionRoutineCallback = base::OnceCallback<void(
      mojom::RoutineVerdict,
      const std::vector<mojom::HasSecureWiFiConnectionProblem>&)>;

  HasSecureWiFiConnectionRoutine();
  ~HasSecureWiFiConnectionRoutine() override;

  // NetworkDiagnosticsRoutine:
  bool CanRun() override;
  void AnalyzeResultsAndExecuteCallback() override;

  void RunTest(HasSecureWiFiConnectionRoutineCallback callback);

 private:
  void FetchActiveWiFiNetworks();
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  HasSecureWiFiConnectionRoutineCallback routine_completed_callback_;
  bool wifi_connected_ = false;
  chromeos::network_config::mojom::SecurityType wifi_security_ =
      chromeos::network_config::mojom::SecurityType::kNone;
  std::vector<mojom::HasSecureWiFiConnectionProblem> problems_;

  DISALLOW_COPY_AND_ASSIGN(HasSecureWiFiConnectionRoutine);
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_HAS_SECURE_WIFI_CONNECTION_ROUTINE_H_
