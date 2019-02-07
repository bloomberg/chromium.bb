// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/browser/browser_switcher/browser_switcher_service_win.h"
#endif

namespace browser_switcher {

namespace {

const char kAValidUrl[] = "http://example.com/";
const char kAnInvalidUrl[] = "the quick brown fox jumps over the lazy dog";

bool ReturnValidXml(content::URLLoaderInterceptor::RequestParams* params) {
  std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
  std::string xml =
      "<rules version=\"1\"><docMode><domain docMode=\"9\">"
      "docs.google.com</domain></docMode></rules>";
  content::URLLoaderInterceptor::WriteResponse(headers, xml,
                                               params->client.get());
  return true;
}

bool FailToDownload(content::URLLoaderInterceptor::RequestParams* params) {
  std::string headers = "HTTP/1.1 500 Internal Server Error\n\n";
  content::URLLoaderInterceptor::WriteResponse(headers, "",
                                               params->client.get());
  return true;
}

bool ShouldSwitch(BrowserSwitcherService* service, const GURL& url) {
  return service->sitelist()->ShouldSwitch(url);
}

void EnableBrowserSwitcher(policy::PolicyMap* policies) {
  policies->Set(policy::key::kBrowserSwitcherEnabled,
                policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                policy::POLICY_SOURCE_PLATFORM,
                std::make_unique<base::Value>(true), nullptr);
}

}  // namespace

class BrowserSwitcherServiceTest : public InProcessBrowserTest {
 public:
  BrowserSwitcherServiceTest() = default;
  ~BrowserSwitcherServiceTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    BrowserSwitcherService::SetFetchDelayForTesting(base::TimeDelta());
  }

  void SetUseIeSitelist(bool use_ie_sitelist) {
    policy::PolicyMap policies;
    EnableBrowserSwitcher(&policies);
    policies.Set(policy::key::kBrowserSwitcherUseIeSitelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_PLATFORM,
                 std::make_unique<base::Value>(use_ie_sitelist), nullptr);
    provider_.UpdateChromePolicy(policies);
    base::RunLoop().RunUntilIdle();
  }

  void SetExternalUrl(const std::string& url) {
    policy::PolicyMap policies;
    EnableBrowserSwitcher(&policies);
    policies.Set(policy::key::kBrowserSwitcherExternalSitelistUrl,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_PLATFORM,
                 std::make_unique<base::Value>(url), nullptr);
    provider_.UpdateChromePolicy(policies);
    base::RunLoop().RunUntilIdle();
  }

 private:
  policy::MockConfigurationPolicyProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSwitcherServiceTest);
};

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, ExternalSitelistInvalidUrl) {
  SetExternalUrl(kAnInvalidUrl);

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams* params) {
        if (!params->url_request.url.is_valid() ||
            params->url_request.url.spec() == kAnInvalidUrl) {
          *happened = true;
        }
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](bool* happened, base::OnceClosure quit) {
            EXPECT_FALSE(*happened);
            std::move(quit).Run();
          },
          &fetch_happened, run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalFetchAndParseAfterStartup) {
  SetExternalUrl(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(ReturnValidXml));

  // Execute everything and make sure the rules are applied correctly.
  auto* service =
      BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](BrowserSwitcherService* service, base::OnceClosure quit) {
            EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
            EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
            std::move(quit).Run();
          },
          service, run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalIgnoresFailedDownload) {
  SetExternalUrl(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(FailToDownload));

  // Execute everything and make sure no rules are applied.
  auto* service =
      BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](BrowserSwitcherService* service, base::OnceClosure quit) {
            EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
            EXPECT_FALSE(
                ShouldSwitch(service, GURL("http://docs.google.com/")));
            std::move(quit).Run();
          },
          service, run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalIgnoresNonManagedPref) {
  browser()->profile()->GetPrefs()->SetString(prefs::kExternalSitelistUrl,
                                              kAValidUrl);

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() == kAValidUrl)
          *happened = true;
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](bool* happened, base::OnceClosure quit) {
            EXPECT_FALSE(*happened);
            std::move(quit).Run();
          },
          &fetch_happened, run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, IeemSitelistInvalidUrl) {
  SetUseIeSitelist(true);
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(kAnInvalidUrl);

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams* params) {
        if (!params->url_request.url.is_valid() ||
            params->url_request.url.spec() == kAnInvalidUrl) {
          *happened = true;
        }
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](bool* happened, base::OnceClosure quit) {
            EXPECT_FALSE(*happened);
            std::move(quit).Run();
          },
          &fetch_happened, run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       IeemFetchAndParseAfterStartup) {
  SetUseIeSitelist(true);
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(ReturnValidXml));

  // Execute everything and make sure the rules are applied correctly.
  auto* service =
      BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](BrowserSwitcherService* service, base::OnceClosure quit) {
            EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
            EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
            std::move(quit).Run();
          },
          service, run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, IeemIgnoresFailedDownload) {
  SetUseIeSitelist(true);
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(FailToDownload));

  // Execute everything and make sure no rules are applied.
  auto* service =
      BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](BrowserSwitcherService* service, base::OnceClosure quit) {
            EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
            EXPECT_FALSE(
                ShouldSwitch(service, GURL("http://docs.google.com/")));
            std::move(quit).Run();
          },
          service, run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, IeemIgnoresNonManagedPref) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUseIeSitelist, true);
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(kAValidUrl);

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() == kAValidUrl)
          *happened = true;
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](bool* happened, base::OnceClosure quit) {
            EXPECT_FALSE(*happened);
            std::move(quit).Run();
          },
          &fetch_happened, run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}
#endif

}  // namespace browser_switcher
