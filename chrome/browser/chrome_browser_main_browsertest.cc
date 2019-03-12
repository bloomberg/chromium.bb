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
#include "content/public/test/network_connection_change_simulator.h"

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

class ChromeBrowserMainBrowserTest : public InProcessBrowserTest {
 public:
  ChromeBrowserMainBrowserTest() {
    net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
    // Since the test currently performs an actual request to localhost (which
    // is expected to fail since no variations server is running), retries are
    // disabled to prevent race conditions from causing flakiness in tests.
    scoped_feature_list_.InitAndDisableFeature(variations::kHttpRetryFeature);
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
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainBrowserTest);
};

// Verifies VariationsService does a request when network status changes from
// none to connected. This is a regression test for https://crbug.com/826930.
// TODO(crbug.com/905714): This test should use a mock variations server
// instead of performing an actual request.
#if defined(OS_CHROMEOS)
#define MAYBE_VariationsServiceStartsRequestOnNetworkChange \
  DISABLED_VariationsServiceStartsRequestOnNetworkChange
#else
#define MAYBE_VariationsServiceStartsRequestOnNetworkChange \
  VariationsServiceStartsRequestOnNetworkChange
#endif
IN_PROC_BROWSER_TEST_F(ChromeBrowserMainBrowserTest,
                       MAYBE_VariationsServiceStartsRequestOnNetworkChange) {
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  variations_service->CancelCurrentRequestForTesting();

  content::NetworkConnectionChangeSimulator network_change_simulator;
  network_change_simulator.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  const int initial_request_count = variations_service->request_count();

  // The variations service will only send a request the first time the
  // connection goes online, or after the 30min delay. Tell it that it hasn't
  // sent a request yet to make sure the next time we go online a request will
  // be sent.
  variations_service->GetResourceRequestAllowedNotifierForTesting()
      ->SetObserverRequestedForTesting(true);

  network_change_simulator.SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  // NotifyObserversOfNetworkChangeForTests uses PostTask, so run the loop until
  // idle to ensure VariationsService processes the network change.
  base::RunLoop().RunUntilIdle();
  const int final_request_count = variations_service->request_count();
  EXPECT_EQ(initial_request_count + 1, final_request_count);
}

}  // namespace
