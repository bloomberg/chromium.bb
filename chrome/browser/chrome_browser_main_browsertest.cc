// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

// Friend of ChromeBrowserMainPartsTestApi to poke at internal state.
class ChromeBrowserMainPartsTestApi {
 public:
  explicit ChromeBrowserMainPartsTestApi(ChromeBrowserMainParts* main_parts)
      : main_parts_(main_parts) {}
  ~ChromeBrowserMainPartsTestApi() = default;

  void EnableVariationsServiceInit() {
    main_parts_
        ->should_call_pre_main_loop_start_startup_on_variations_service_ = true;
  }

 private:
  ChromeBrowserMainParts* main_parts_;
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsTestApi);
};

namespace {

class NetworkConnectionChangeSimulator
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  NetworkConnectionChangeSimulator() = default;

  void SetConnectionType(network::mojom::ConnectionType type) {
    network::NetworkConnectionTracker* network_connection_tracker =
        content::GetNetworkConnectionTracker();
    network::mojom::ConnectionType connection_type =
        network::mojom::ConnectionType::CONNECTION_UNKNOWN;
    run_loop_ = std::make_unique<base::RunLoop>();
    network_connection_tracker->AddNetworkConnectionObserver(this);
    SimulateNetworkChange(type);
    // Make sure the underlying network connection type becomes |type|.
    // The while loop is necessary because in some machine such as "Builder
    // linux64 trunk", the |connection_type| can be CONNECTION_ETHERNET before
    // it changes to |type|. So here it needs to wait until the
    // |connection_type| becomes |type|.
    while (!network_connection_tracker->GetConnectionType(
               &connection_type,
               base::BindOnce(
                   &NetworkConnectionChangeSimulator::OnConnectionChanged,
                   base::Unretained(this))) ||
           connection_type != type) {
      SimulateNetworkChange(type);
      run_loop_->Run();
      run_loop_ = std::make_unique<base::RunLoop>();
    }
    network_connection_tracker->RemoveNetworkConnectionObserver(this);
  }

 private:
  // Simulates a network connection change.
  static void SimulateNetworkChange(network::mojom::ConnectionType type) {
    if (base::FeatureList::IsEnabled(network::features::kNetworkService) &&
        !content::IsNetworkServiceRunningInProcess()) {
      network::mojom::NetworkServiceTestPtr network_service_test;
      content::ServiceManagerConnection::GetForProcess()
          ->GetConnector()
          ->BindInterface(content::mojom::kNetworkServiceName,
                          &network_service_test);
      base::RunLoop run_loop;
      network_service_test->SimulateNetworkChange(type, run_loop.QuitClosure());
      run_loop.Run();
      return;
    }
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        net::NetworkChangeNotifier::ConnectionType(type));
  }

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    run_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionChangeSimulator);
};

// ChromeBrowserMainExtraParts is used to initialize the network state.
class ChromeBrowserMainExtraPartsNetFactoryInstaller
    : public ChromeBrowserMainExtraParts {
 public:
  explicit ChromeBrowserMainExtraPartsNetFactoryInstaller(
      NetworkConnectionChangeSimulator* network_change_simulator)
      : network_change_simulator_(network_change_simulator) {
    EXPECT_TRUE(network_change_simulator_);
  }

  // ChromeBrowserMainExtraParts:
  void PreEarlyInitialization() override {}
  void ServiceManagerConnectionStarted(
      content::ServiceManagerConnection* connection) override {
    network_change_simulator_->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_NONE);
  }

 private:
  NetworkConnectionChangeSimulator* network_change_simulator_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsNetFactoryInstaller);
};

class ChromeBrowserMainBrowserTest : public InProcessBrowserTest {
 public:
  ChromeBrowserMainBrowserTest() = default;

 protected:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Without this (and EnableFetchForTesting() below) VariationsService won't
    // do requests in non-branded builds.
    command_line->AppendSwitchASCII(variations::switches::kVariationsServerURL,
                                    "http://localhost");
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    variations::VariationsService::EnableFetchForTesting();
    ChromeBrowserMainParts* chrome_browser_main_parts =
        static_cast<ChromeBrowserMainParts*>(browser_main_parts);
    ChromeBrowserMainPartsTestApi(chrome_browser_main_parts)
        .EnableVariationsServiceInit();
    network_change_simulator_ =
        std::make_unique<NetworkConnectionChangeSimulator>();
    extra_parts_ = new ChromeBrowserMainExtraPartsNetFactoryInstaller(
        network_change_simulator_.get());
    chrome_browser_main_parts->AddParts(extra_parts_);
  }

  std::unique_ptr<NetworkConnectionChangeSimulator> network_change_simulator_;
  ChromeBrowserMainExtraPartsNetFactoryInstaller* extra_parts_ = nullptr;

 private:

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainBrowserTest);
};

// Verifies VariationsService does a request when network status changes from
// none to connected. This is a regression test for https://crbug.com/826930.
IN_PROC_BROWSER_TEST_F(ChromeBrowserMainBrowserTest,
                       VariationsServiceStartsRequestOnNetworkChange) {
  const int initial_request_count =
      g_browser_process->variations_service()->request_count();
  ASSERT_TRUE(extra_parts_);
  network_change_simulator_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  // NotifyObserversOfNetworkChangeForTests uses PostTask, so run the loop until
  // idle to ensure VariationsService processes the network change.
  base::RunLoop().RunUntilIdle();
  const int final_request_count =
      g_browser_process->variations_service()->request_count();
  EXPECT_EQ(initial_request_count + 1, final_request_count);
}

}  // namespace
