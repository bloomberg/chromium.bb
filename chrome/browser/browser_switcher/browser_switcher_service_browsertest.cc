// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include <string.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
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
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_win.h"
#endif

namespace browser_switcher {

namespace {

const char kAValidUrl[] = "http://example.com/";
const char kAnInvalidUrl[] = "the quick brown fox jumps over the lazy dog";

const char kSitelistXml[] =
    "<rules version=\"1\"><docMode><domain docMode=\"9\">"
    "docs.google.com</domain></docMode></rules>";

const char kOtherSitelistXml[] =
    "<rules version=\"1\"><docMode><domain docMode=\"9\">"
    "yahoo.com</domain></docMode></rules>";

bool ReturnValidXml(content::URLLoaderInterceptor::RequestParams* params) {
  std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
  content::URLLoaderInterceptor::WriteResponse(
      headers, std::string(kSitelistXml), params->client.get());
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

void SetPolicy(policy::PolicyMap* policies,
               const char* key,
               std::unique_ptr<base::Value> value) {
  policies->Set(key, policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                policy::POLICY_SOURCE_PLATFORM, std::move(value), nullptr);
}

void EnableBrowserSwitcher(policy::PolicyMap* policies) {
  SetPolicy(policies, policy::key::kBrowserSwitcherEnabled,
            std::make_unique<base::Value>(true));
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
    BrowserSwitcherService::SetRefreshDelayForTesting(
        TestTimeouts::action_timeout() * 3 / 2);
#if defined(OS_WIN)
    ASSERT_TRUE(fake_appdata_dir_.CreateUniqueTempDir());
    base::PathService::Override(base::DIR_LOCAL_APP_DATA,
                                fake_appdata_dir_.GetPath());
#endif
  }

  void SetUseIeSitelist(bool use_ie_sitelist) {
    policy::PolicyMap policies;
    EnableBrowserSwitcher(&policies);
    SetPolicy(&policies, policy::key::kBrowserSwitcherUseIeSitelist,
              std::make_unique<base::Value>(use_ie_sitelist));
    provider_.UpdateChromePolicy(policies);
    base::RunLoop().RunUntilIdle();
  }

  void SetExternalUrl(const std::string& url) {
    policy::PolicyMap policies;
    EnableBrowserSwitcher(&policies);
    SetPolicy(&policies, policy::key::kBrowserSwitcherExternalSitelistUrl,
              std::make_unique<base::Value>(url));
    provider_.UpdateChromePolicy(policies);
    base::RunLoop().RunUntilIdle();
  }

  policy::MockConfigurationPolicyProvider& policy_provider() {
    return provider_;
  }

#if defined(OS_WIN)
  const base::FilePath& appdata_dir() { return fake_appdata_dir_.GetPath(); }
#endif

 private:
  policy::MockConfigurationPolicyProvider provider_;

#if defined(OS_WIN)
  base::ScopedTempDir fake_appdata_dir_;
#endif

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

  int counter = 0;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](int* counter, content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() != kAValidUrl)
          return false;
        // Return a different sitelist on refresh.
        const char* sitelist_xml =
            (*counter == 0) ? kSitelistXml : kOtherSitelistXml;
        std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
        content::URLLoaderInterceptor::WriteResponse(
            headers, std::string(sitelist_xml), params->client.get());
        (*counter)++;
        return true;
      },
      &counter));

  // Execute everything and make sure the rules are applied correctly.
  auto* service =
      BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](BrowserSwitcherService* service) {
            EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
            EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
            EXPECT_FALSE(ShouldSwitch(service, GURL("http://yahoo.com/")));
          },
          service),
      TestTimeouts::action_timeout());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](BrowserSwitcherService* service, base::OnceClosure quit) {
            EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
            EXPECT_FALSE(
                ShouldSwitch(service, GURL("http://docs.google.com/")));
            EXPECT_TRUE(ShouldSwitch(service, GURL("http://yahoo.com/")));
            std::move(quit).Run();
          },
          service, run_loop.QuitClosure()),
      TestTimeouts::action_timeout() * 2);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalFirstFetchFailsButSecondWorks) {
  SetExternalUrl(kAValidUrl);

  int counter = 0;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](int* counter, content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() != kAValidUrl)
          return false;
        // First request fails, but second succeeds.
        if (*counter == 0)
          FailToDownload(params);
        else
          ReturnValidXml(params);
        (*counter)++;
        return true;
      },
      &counter));

  // Execute everything and make sure the rules are applied correctly.
  auto* service =
      BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](BrowserSwitcherService* service) {
            EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
            EXPECT_FALSE(
                ShouldSwitch(service, GURL("http://docs.google.com/")));
            EXPECT_FALSE(ShouldSwitch(service, GURL("http://yahoo.com/")));
          },
          service),
      TestTimeouts::action_timeout());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](BrowserSwitcherService* service, base::OnceClosure quit) {
            EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
            EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
            std::move(quit).Run();
          },
          service, run_loop.QuitClosure()),
      TestTimeouts::action_timeout() * 2);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, ExternalFileUrl) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath sitelist_path = dir.GetPath().AppendASCII("sitelist.xml");
  base::WriteFile(sitelist_path, kSitelistXml, strlen(kSitelistXml));

  SetExternalUrl(net::FilePathToFileURL(sitelist_path).spec());

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

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, WritesPrefsToCacheFile) {
  policy::PolicyMap policies;
  EnableBrowserSwitcher(&policies);
  SetPolicy(&policies, policy::key::kAlternativeBrowserPath,
            std::make_unique<base::Value>("IExplore.exe"));
  auto alt_params = std::make_unique<base::ListValue>();
  alt_params->GetList().push_back(base::Value("--bogus-flag"));
  SetPolicy(&policies, policy::key::kAlternativeBrowserParameters,
            std::move(alt_params));
  SetPolicy(&policies, policy::key::kBrowserSwitcherChromePath,
            std::make_unique<base::Value>("chrome.exe"));
  auto chrome_params = std::make_unique<base::ListValue>();
  chrome_params->GetList().push_back(base::Value("--force-dark-mode"));
  SetPolicy(&policies, policy::key::kBrowserSwitcherChromeParameters,
            std::move(chrome_params));
  auto url_list = std::make_unique<base::ListValue>();
  url_list->GetList().push_back(base::Value("example.com"));
  SetPolicy(&policies, policy::key::kBrowserSwitcherUrlList,
            std::move(url_list));
  auto greylist = std::make_unique<base::ListValue>();
  greylist->GetList().push_back(base::Value("foo.example.com"));
  SetPolicy(&policies, policy::key::kBrowserSwitcherUrlGreylist,
            std::move(greylist));
  policy_provider().UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  base::FilePath cache_file_path = appdata_dir()
                                       .AppendASCII("Google")
                                       .AppendASCII("BrowserSwitcher")
                                       .AppendASCII("cache.dat");

  // Execute everything and check "cache.dat" file contents.
  BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath path, base::OnceClosure quit) {
            base::ScopedAllowBlockingForTesting allow_blocking;
            base::File file(path,
                            base::File::FLAG_OPEN | base::File::FLAG_READ);
            ASSERT_TRUE(file.IsValid());

            const char expected_output[] =
                "1\n"
                "IExplore.exe\n"
                "--bogus-flag\n"
                "chrome.exe\n"
                "--force-dark-mode\n"
                "1\n"
                "example.com\n"
                "1\n"
                "foo.example.com\n";

            std::unique_ptr<char[]> buffer(new char[file.GetLength() + 1]);
            buffer.get()[file.GetLength()] = '\0';
            file.Read(0, buffer.get(), file.GetLength());
            // Check that there's no space in the URL (i.e. replaced with %20).
            EXPECT_EQ(std::string(expected_output), std::string(buffer.get()));

            std::move(quit).Run();
          },
          cache_file_path, run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, WritesSitelistsToCacheFile) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath ieem_sitelist_path =
      dir.GetPath().AppendASCII("ieem_sitelist.xml");
  base::WriteFile(ieem_sitelist_path, kSitelistXml, strlen(kSitelistXml));

  base::FilePath external_sitelist_path =
      dir.GetPath().AppendASCII("external_sitelist.xml");
  base::WriteFile(external_sitelist_path, kOtherSitelistXml,
                  strlen(kOtherSitelistXml));

  policy::PolicyMap policies;
  EnableBrowserSwitcher(&policies);
  SetPolicy(&policies, policy::key::kBrowserSwitcherExternalSitelistUrl,
            std::make_unique<base::Value>(
                net::FilePathToFileURL(external_sitelist_path).spec()));
  SetPolicy(&policies, policy::key::kBrowserSwitcherUseIeSitelist,
            std::make_unique<base::Value>(true));
  policy_provider().UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();
  BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(
      net::FilePathToFileURL(ieem_sitelist_path).spec());

  base::FilePath cache_file_path = appdata_dir()
                                       .AppendASCII("Google")
                                       .AppendASCII("BrowserSwitcher")
                                       .AppendASCII("sitelistcache.dat");

  // Execute everything and check "sitelistcache.dat" file contents. It should
  // contain the *union* of both sitelists, not just one of them.
  BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath path, base::OnceClosure quit) {
            base::ScopedAllowBlockingForTesting allow_blocking;
            base::File file(path,
                            base::File::FLAG_OPEN | base::File::FLAG_READ);
            ASSERT_TRUE(file.IsValid());

            const char expected_output[] =
                "1\n"
                "2\n"
                "docs.google.com\n"
                "yahoo.com\n";

            std::unique_ptr<char[]> buffer(new char[file.GetLength() + 1]);
            buffer.get()[file.GetLength()] = '\0';
            file.Read(0, buffer.get(), file.GetLength());
            // Check that there's no space in the URL (i.e. replaced with %20).
            EXPECT_EQ(std::string(expected_output), std::string(buffer.get()));

            std::move(quit).Run();
          },
          cache_file_path, run_loop.QuitClosure()),
      TestTimeouts::action_timeout());
  run_loop.Run();
}
#endif

}  // namespace browser_switcher
