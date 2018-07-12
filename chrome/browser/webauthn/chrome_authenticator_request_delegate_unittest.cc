// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include "build/build_config.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class ChromeAuthenticatorRequestDelegateTest
    : public ChromeRenderViewHostTestHarness {};

#if defined(OS_MACOSX)
std::string TouchIdMetadataSecret(
    const ChromeAuthenticatorRequestDelegate& delegate) {
  base::Optional<AuthenticatorRequestClientDelegate::TouchIdAuthenticatorConfig>
      config = delegate.GetTouchIdAuthenticatorConfig();
  return config->metadata_secret;
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, TouchIdMetadataSecret) {
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  std::string secret = TouchIdMetadataSecret(delegate);
  EXPECT_EQ(secret.size(), 32u);
  EXPECT_EQ(secret, TouchIdMetadataSecret(delegate));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_EqualForSameProfile) {
  // Different delegates on the same BrowserContext (Profile) should return the
  // same secret.
  EXPECT_EQ(
      TouchIdMetadataSecret(ChromeAuthenticatorRequestDelegate(main_rfh())),
      TouchIdMetadataSecret(ChromeAuthenticatorRequestDelegate(main_rfh())));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_NotEqualForDifferentProfiles) {
  // Different profiles have different secrets. (No way to reset
  // browser_context(), so we have to create our own.)
  auto browser_context = base::WrapUnique(CreateBrowserContext());
  auto web_contents =
      WebContentsTester::CreateTestWebContents(browser_context.get(), nullptr);
  EXPECT_NE(
      TouchIdMetadataSecret(ChromeAuthenticatorRequestDelegate(main_rfh())),
      TouchIdMetadataSecret(
          ChromeAuthenticatorRequestDelegate(web_contents->GetMainFrame())));
  // Ensure this second secret is actually valid.
  EXPECT_EQ(32u, TouchIdMetadataSecret(ChromeAuthenticatorRequestDelegate(
                                           web_contents->GetMainFrame()))
                     .size());
}
#endif

}  // namespace
}  // namespace content
