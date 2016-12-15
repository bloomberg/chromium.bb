// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ShareServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  ShareServiceTest() = default;
  ~ShareServiceTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ShareServiceImpl::Create(mojo::GetProxy(&share_service_));
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void DidShare(const base::Optional<std::string>& expected,
                const base::Optional<std::string>& str) {
    EXPECT_EQ(expected, str);

    if (!on_callback_.is_null())
      on_callback_.Run();
  }

  blink::mojom::ShareServicePtr share_service_;
  base::Closure on_callback_;
};

// Basic test to check the Share method calls the callback with the expected
// parameters.
TEST_F(ShareServiceTest, ShareCallbackParams) {
  const GURL url("https://www.google.com");

  base::RunLoop run_loop;
  on_callback_ = run_loop.QuitClosure();

  base::Callback<void(const base::Optional<std::string>&)> callback =
      base::Bind(
          &ShareServiceTest::DidShare, base::Unretained(this),
          base::Optional<std::string>("Not implemented: navigator.share"));
  share_service_->Share("title", "text", url, callback);

  run_loop.Run();
}
