// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"
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

// Simulates a network connection change.
void SimulateNetworkChange(network::mojom::ConnectionType type) {
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

// ChromeBrowserMainExtraParts used to install a MockNetworkChangeNotifier.
class ChromeBrowserMainExtraPartsNetFactoryInstaller
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsNetFactoryInstaller(bool migration_enabled)
      : migration_enabled_(migration_enabled) {}
  ~ChromeBrowserMainExtraPartsNetFactoryInstaller() override {
    // |network_change_notifier_| needs to be destroyed before |net_installer_|.
    network_change_notifier_.reset();
  }

  net::test::MockNetworkChangeNotifier* network_change_notifier() {
    return network_change_notifier_.get();
  }

  // ChromeBrowserMainExtraParts:
  void PreEarlyInitialization() override {}
  void ServiceManagerConnectionStarted(
      content::ServiceManagerConnection* connection) override {
    if (!migration_enabled_)
      return;
    SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_NONE);
  }
  void PostMainMessageLoopStart() override {
    if (migration_enabled_)
      return;
    ASSERT_TRUE(net::NetworkChangeNotifier::HasNetworkChangeNotifier());
    net_installer_ =
        std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
    network_change_notifier_ =
        std::make_unique<net::test::MockNetworkChangeNotifier>();
    network_change_notifier_->SetConnectionType(
        net::NetworkChangeNotifier::CONNECTION_NONE);
  }

 private:
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier_;
  std::unique_ptr<net::NetworkChangeNotifier::DisableForTest> net_installer_;
  bool migration_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsNetFactoryInstaller);
};

enum class MigrationState {
  kEnabled,
  kDisabled,
};

class ChromeBrowserMainBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<MigrationState>,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  ChromeBrowserMainBrowserTest() {
    if (GetParam() == MigrationState::kEnabled) {
      feature_list_.InitAndEnableFeature(
          web_resource::kResourceRequestAllowedMigration);
    }
  }

  ~ChromeBrowserMainBrowserTest() override {
    content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
        this);
  }

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
    extra_parts_ = new ChromeBrowserMainExtraPartsNetFactoryInstaller(
        GetParam() == MigrationState::kEnabled);
    chrome_browser_main_parts->AddParts(extra_parts_);
  }

  void SetUpOnMainThread() override {
    content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  }

  void WaitForConnectionType(network::mojom::ConnectionType type) {
    if (connection_type_ == type)
      return;

    expected_connection_type_ = type;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  ChromeBrowserMainExtraPartsNetFactoryInstaller* extra_parts_ = nullptr;

 private:
  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    connection_type_ = type;
    if (expected_connection_type_ == connection_type_ && run_loop_)
      run_loop_->Quit();
  }

  base::test::ScopedFeatureList feature_list_;
  network::mojom::ConnectionType expected_connection_type_ =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  network::mojom::ConnectionType connection_type_ =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainBrowserTest);
};

// Verifies VariationsService does a request when network status changes from
// none to connected. This is a regression test for https://crbug.com/826930.
IN_PROC_BROWSER_TEST_P(ChromeBrowserMainBrowserTest,
                       VariationsServiceStartsRequestOnNetworkChange) {
  const int initial_request_count =
      g_browser_process->variations_service()->request_count();
  ASSERT_TRUE(extra_parts_);
  if (GetParam() == MigrationState::kEnabled) {
    SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_WIFI);
    WaitForConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  } else {
    extra_parts_->network_change_notifier()->SetConnectionType(
        net::NetworkChangeNotifier::CONNECTION_WIFI);
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        net::NetworkChangeNotifier::CONNECTION_WIFI);
  }
  // NotifyObserversOfNetworkChangeForTests uses PostTask, so run the loop until
  // idle to ensure VariationsService processes the network change.
  base::RunLoop().RunUntilIdle();
  const int final_request_count =
      g_browser_process->variations_service()->request_count();
  EXPECT_EQ(initial_request_count + 1, final_request_count);
}

INSTANTIATE_TEST_CASE_P(,
                        ChromeBrowserMainBrowserTest,
                        testing::Values(MigrationState::kEnabled,
                                        MigrationState::kDisabled));

}  // namespace
