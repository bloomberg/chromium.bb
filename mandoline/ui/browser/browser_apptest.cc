// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/browser.h"

#include "base/run_loop.h"
#include "components/view_manager/public/cpp/view.h"
#include "mandoline/ui/browser/browser_delegate.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"

namespace mandoline {

class TestBrowser : public Browser {
 public:
  TestBrowser(mojo::ApplicationImpl* app, BrowserDelegate* delegate)
    : Browser(app, delegate, GURL()) {}
  ~TestBrowser() override {}

  void WaitForOnEmbed() {
    if (root_)
      return;
    embed_run_loop_.reset(new base::RunLoop);
    embed_run_loop_->Run();
    embed_run_loop_.reset();
  }

  mojo::View* root() { return root_; }

 private:
  // Overridden from Browser:
  void OnEmbed(mojo::View* root) override {
    // Don't call the base class because we don't want to navigate.
    CHECK(!root_);
    root_ = root;
    if (embed_run_loop_)
      embed_run_loop_->Quit();
  }

  // If non-null we're waiting for OnEmbed() using this RunLoop.
  scoped_ptr<base::RunLoop> embed_run_loop_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(TestBrowser);
};

class BrowserTest : public mojo::test::ApplicationTestBase,
                    public mojo::ApplicationDelegate,
                    public BrowserDelegate {
 public:
  BrowserTest()
      : app_(nullptr),
        last_browser_closed_(nullptr) {}

  // Creates a new Browser object.
  TestBrowser* CreateBrowser() {
    if (!app_)
      return nullptr;
    TestBrowser* browser = new TestBrowser(app_, this);
    browsers_.insert(browser);
    return browser;
  }

  TestBrowser* WaitForBrowserClosed() {
    if (!last_browser_closed_) {
      browser_closed_run_loop_.reset(new base::RunLoop);
      browser_closed_run_loop_->Run();
      browser_closed_run_loop_.reset();
    }
    TestBrowser* last_browser = last_browser_closed_;
    last_browser_closed_ = nullptr;
    return last_browser;
  }

  // Overridden from ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {
    app_ = app;
  }

  // ApplicationTestBase:
  ApplicationDelegate* GetApplicationDelegate() override { return this; }

  // Overridden from BrowserDelegate:
  void BrowserClosed(Browser* browser) override {
    scoped_ptr<Browser> browser_owner(browser);
    TestBrowser* test_browser = static_cast<TestBrowser*>(browser);
    DCHECK_GT(browsers_.count(test_browser), 0u);
    browsers_.erase(test_browser);
    last_browser_closed_ = test_browser;
    if (browser_closed_run_loop_) {
      browser_owner.reset();
      browser_closed_run_loop_->Quit();
    }
  }

  void InitUIIfNecessary(Browser* browser, mojo::View* root_view) override {}

 private:
  mojo::ApplicationImpl* app_;
  std::set<TestBrowser*> browsers_;
  TestBrowser* last_browser_closed_;
  scoped_ptr<base::RunLoop> browser_closed_run_loop_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(BrowserTest);
};

// This test verifies that closing a Browser closes the associated application
// connection with the view manager.
TEST_F(BrowserTest, ClosingBrowserClosesAppConnection) {
  Browser* browser = CreateBrowser();
  ASSERT_NE(nullptr, browser);
  mojo::ApplicationConnection* view_manager_connection =
      browser->view_manager_init_.connection();
  mojo::ApplicationConnection::TestApi connection_test_api(
      view_manager_connection);
  ASSERT_NE(nullptr, view_manager_connection);
  base::WeakPtr<mojo::ApplicationConnection> ptr =
      connection_test_api.GetWeakPtr();
  BrowserClosed(browser);
  EXPECT_FALSE(ptr);
}

// This test verifies that we can create two Browsers and each Browser has a
// different AppliationConnection and different root view.
TEST_F(BrowserTest, TwoBrowsers) {
  TestBrowser* browser1 = CreateBrowser();
  mojo::ApplicationConnection* browser1_connection =
      browser1->view_manager_init_.connection();
  ASSERT_NE(nullptr, browser1);
  browser1->WaitForOnEmbed();

  TestBrowser* browser2 = CreateBrowser();
  mojo::ApplicationConnection* browser2_connection =
    browser2->view_manager_init_.connection();
  ASSERT_NE(nullptr, browser2);
  browser2->WaitForOnEmbed();

  // Verify that we have two different connections to the ViewManager.
  ASSERT_NE(browser1_connection, browser2_connection);

  // Verify that we have two different root nodes.
  ASSERT_NE(browser1->root(), browser2->root());

  // Deleting the view manager closes the connection.
  delete browser1->root()->view_manager();
  EXPECT_EQ(browser1, WaitForBrowserClosed());

  delete browser2->root()->view_manager();
  EXPECT_EQ(browser2, WaitForBrowserClosed());
}

}  // namespace mandoline
