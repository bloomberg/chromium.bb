// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::TestBrowserThread;
using content::WebContents;
using content::WebContentsTester;
using extensions::WebAuthFlow;
using testing::Return;
using testing::ReturnRef;

namespace {

class MockDelegate : public WebAuthFlow::Delegate {
 public:
  MOCK_METHOD1(OnAuthFlowSuccess, void(const std::string& redirect_url));
  MOCK_METHOD0(OnAuthFlowFailure, void());
};

class MockWebAuthFlow : public WebAuthFlow {
 public:
  MockWebAuthFlow(
     WebAuthFlow::Delegate* delegate,
     Profile* profile,
     const std::string& extension_id,
     const GURL& provider_url,
     bool interactive)
     : WebAuthFlow(
           delegate,
           profile,
           extension_id,
           provider_url,
           interactive ? WebAuthFlow::INTERACTIVE : WebAuthFlow::SILENT,
           gfx::Rect(),
           chrome::GetActiveDesktop()),
       profile_(profile),
       web_contents_(NULL),
       window_shown_(false) { }

  virtual WebContents* CreateWebContents() OVERRIDE {
    CHECK(!web_contents_);
    web_contents_ = WebContentsTester::CreateTestWebContents(profile_, NULL);
    return web_contents_;
  }

  virtual void ShowAuthFlowPopup() OVERRIDE {
    window_shown_ = true;
  }

  bool HasWindow() const {
    return window_shown_;
  }

  void DestroyWebContents() {
    CHECK(web_contents_);
    delete web_contents_;
  }

  virtual ~MockWebAuthFlow() { }

 private:
  Profile* profile_;
  WebContents* web_contents_;
  bool window_shown_;
};

}  // namespace

class WebAuthFlowTest : public ChromeRenderViewHostTestHarness {
 protected:
  WebAuthFlowTest()
      : thread_(BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
  }

  virtual void TearDown() {
    // |flow_| must be reset before ChromeRenderViewHostTestHarness::TearDown(),
    // because |flow_| deletes the WebContents it owns via
    // MessageLoop::DeleteSoon().
    flow_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateAuthFlow(const std::string& extension_id,
                      const GURL& url,
                      bool interactive) {
    flow_.reset(new MockWebAuthFlow(
        &delegate_, profile(), extension_id, url, interactive));
  }

  WebAuthFlow* flow_base() {
    return flow_.get();
  }

  bool CallBeforeUrlLoaded(const GURL& url) {
    return flow_base()->BeforeUrlLoaded(url);
  }

  void CallAfterUrlLoaded() {
    flow_base()->AfterUrlLoaded();
  }

  bool CallIsValidRedirectUrl(const GURL& url) {
    return flow_base()->IsValidRedirectUrl(url);
  }

  TestBrowserThread thread_;
  MockDelegate delegate_;
  scoped_ptr<MockWebAuthFlow> flow_;
};

TEST_F(WebAuthFlowTest, SilentRedirectToChromiumAppUrlNonInteractive) {
  std::string ext_id = "abcdefghij";
  GURL url("https://accounts.google.com/o/oauth2/auth");
  GURL result("https://abcdefghij.chromiumapp.org/google_cb");

  CreateAuthFlow(ext_id, url, false);
  EXPECT_CALL(delegate_, OnAuthFlowSuccess(result.spec())).Times(1);
  flow_->Start();
  CallBeforeUrlLoaded(result);
}

TEST_F(WebAuthFlowTest, SilentRedirectToChromiumAppUrlInteractive) {
  std::string ext_id = "abcdefghij";
  GURL url("https://accounts.google.com/o/oauth2/auth");
  GURL result("https://abcdefghij.chromiumapp.org/google_cb");

  CreateAuthFlow(ext_id, url, true);
  EXPECT_CALL(delegate_, OnAuthFlowSuccess(result.spec())).Times(1);
  flow_->Start();
  CallBeforeUrlLoaded(result);
}

TEST_F(WebAuthFlowTest, SilentRedirectToChromeExtensionSchemeUrl) {
  std::string ext_id = "abcdefghij";
  GURL url("https://accounts.google.com/o/oauth2/auth");
  GURL result("chrome-extension://abcdefghij/google_cb");

  CreateAuthFlow(ext_id, url, true);
  EXPECT_CALL(delegate_, OnAuthFlowSuccess(result.spec())).Times(1);
  flow_->Start();
  CallBeforeUrlLoaded(result);
}

TEST_F(WebAuthFlowTest, NeedsUIButNonInteractive) {
  std::string ext_id = "abcdefghij";
  GURL url("https://accounts.google.com/o/oauth2/auth");

  CreateAuthFlow(ext_id, url, false);
  EXPECT_CALL(delegate_, OnAuthFlowFailure()).Times(1);
  flow_->Start();
  CallAfterUrlLoaded();
}

TEST_F(WebAuthFlowTest, UIResultsInSuccess) {
  std::string ext_id = "abcdefghij";
  GURL url("https://accounts.google.com/o/oauth2/auth");
  GURL result("chrome-extension://abcdefghij/google_cb");

  CreateAuthFlow(ext_id, url, true);
  EXPECT_CALL(delegate_, OnAuthFlowSuccess(result.spec())).Times(1);
  flow_->Start();
  CallAfterUrlLoaded();
  EXPECT_TRUE(flow_->HasWindow());
  CallBeforeUrlLoaded(result);
}

TEST_F(WebAuthFlowTest, UIClosedByUser) {
  std::string ext_id = "abcdefghij";
  GURL url("https://accounts.google.com/o/oauth2/auth");
  GURL result("chrome-extension://abcdefghij/google_cb");

  CreateAuthFlow(ext_id, url, true);
  EXPECT_CALL(delegate_, OnAuthFlowFailure()).Times(1);
  flow_->Start();
  CallAfterUrlLoaded();
  EXPECT_TRUE(flow_->HasWindow());
  flow_->DestroyWebContents();
}

TEST_F(WebAuthFlowTest, IsValidRedirectUrl) {
  std::string ext_id = "abcdefghij";
  GURL url("https://accounts.google.com/o/oauth2/auth");

  CreateAuthFlow(ext_id, url, false);

  // Positive cases.
  EXPECT_TRUE(CallIsValidRedirectUrl(
      GURL("https://abcdefghij.chromiumapp.org/")));
  EXPECT_TRUE(CallIsValidRedirectUrl(
      GURL("https://abcdefghij.chromiumapp.org/callback")));
  EXPECT_TRUE(CallIsValidRedirectUrl(
      GURL("chrome-extension://abcdefghij/")));
  EXPECT_TRUE(CallIsValidRedirectUrl(
      GURL("chrome-extension://abcdefghij/callback")));

  // Negative cases.
  EXPECT_FALSE(CallIsValidRedirectUrl(
      GURL("https://www.foo.com/")));
  // http scheme is not allowed.
  EXPECT_FALSE(CallIsValidRedirectUrl(
      GURL("http://abcdefghij.chromiumapp.org/callback")));
  EXPECT_FALSE(CallIsValidRedirectUrl(
      GURL("https://abcd.chromiumapp.org/callback")));
  EXPECT_FALSE(CallIsValidRedirectUrl(
      GURL("chrome-extension://abcd/callback")));
  EXPECT_FALSE(CallIsValidRedirectUrl(
      GURL("chrome-extension://abcdefghijkl/")));
}
