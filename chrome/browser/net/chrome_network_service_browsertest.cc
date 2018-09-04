// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/cookie_config/cookie_store_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "services/network/public/cpp/features.h"

namespace content {
namespace {

constexpr char kCookieName[] = "Name";
constexpr char kCookieValue[] = "Value";

net::CookieList GetCookies(
    const network::mojom::CookieManagerPtr& cookie_manager) {
  base::RunLoop run_loop;
  net::CookieList cookies_out;
  cookie_manager->GetAllCookies(
      base::BindLambdaForTesting([&](const net::CookieList& cookies) {
        cookies_out = cookies;
        run_loop.Quit();
      }));
  run_loop.Run();
  return cookies_out;
}

void SetCookie(const network::mojom::CookieManagerPtr& cookie_manager) {
  base::Time t = base::Time::Now();
  net::CanonicalCookie cookie(kCookieName, kCookieValue, "www.test.com", "/", t,
                              t + base::TimeDelta::FromDays(1), base::Time(),
                              false, false, net::CookieSameSite::DEFAULT_MODE,
                              net::COOKIE_PRIORITY_DEFAULT);
  base::RunLoop run_loop;
  cookie_manager->SetCanonicalCookie(
      cookie, false, false,
      base::BindLambdaForTesting([&](bool success) { run_loop.Quit(); }));
  run_loop.Run();
}

// See |NetworkServiceBrowserTest| for content's version of tests.
class ChromeNetworkServiceBrowserTest : public InProcessBrowserTest {
 public:
  ChromeNetworkServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        network::features::kNetworkService);
  }

 protected:
  network::mojom::NetworkContextPtr CreateNetworkContext(
      bool enable_encrypted_cookies) {
    network::mojom::NetworkContextPtr network_context;
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    context_params->enable_encrypted_cookies = enable_encrypted_cookies;
    context_params->cookie_path =
        browser()->profile()->GetPath().Append(FILE_PATH_LITERAL("cookies"));
    GetNetworkService()->CreateNetworkContext(
        mojo::MakeRequest(&network_context), std::move(context_params));
    return network_context;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkServiceBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ChromeNetworkServiceBrowserTest, PRE_EncryptedCookies) {
  // First set a cookie with cookie encryption enabled.
  network::mojom::NetworkContextPtr context =
      CreateNetworkContext(/*enable_encrypted_cookies=*/true);
  network::mojom::CookieManagerPtr cookie_manager;
  context->GetCookieManager(mojo::MakeRequest(&cookie_manager));

  SetCookie(cookie_manager);

  net::CookieList cookies = GetCookies(cookie_manager);
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ(kCookieValue, cookies[0].Value());
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// The cookies.size() ASSERT is failing flakily on the Win7 bots.
// See https://crbug.com/868667
#define MAYBE_EncryptedCookies DISABLED_EncryptedCookies
#else
#define MAYBE_EncryptedCookies EncryptedCookies
#endif

IN_PROC_BROWSER_TEST_F(ChromeNetworkServiceBrowserTest,
                       MAYBE_EncryptedCookies) {
  net::CookieCryptoDelegate* crypto_delegate =
      cookie_config::GetCookieCryptoDelegate();
  std::string ciphertext;
  crypto_delegate->EncryptString(kCookieValue, &ciphertext);
  // These checks are only valid if crypto is enabled on the platform.
  if (!crypto_delegate->ShouldEncrypt() || ciphertext == kCookieValue)
    return;

  // Now attempt to read the cookie with encryption disabled.
  network::mojom::NetworkContextPtr context =
      CreateNetworkContext(/*enable_encrypted_cookies=*/false);
  network::mojom::CookieManagerPtr cookie_manager;
  context->GetCookieManager(mojo::MakeRequest(&cookie_manager));

  net::CookieList cookies = GetCookies(cookie_manager);
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ("", cookies[0].Value());
}

}  // namespace
}  // namespace content
