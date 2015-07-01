// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/browser.h"

#include "mandoline/ui/browser/browser_delegate.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"

namespace mandoline {

class BrowserTest : public mojo::test::ApplicationTestBase,
                    public mojo::ApplicationDelegate,
                    public BrowserDelegate {
 public:
  BrowserTest() : app_(nullptr), last_closed_connection_(nullptr) {}

  // Creates a new Browser object.
  Browser* CreateBrowser() {
    if (!app_)
      return nullptr;
    Browser* browser = new Browser(app_, this);
    browsers_.insert(browser);
    return browser;
  }

  // Returns the last ApplicationConnection closed.
  void* last_closed_connection() {
    return last_closed_connection_;
  }

  // Overridden from ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {
    app_ = app;
  }

  void OnWillCloseConnection(mojo::ApplicationConnection* connection) override {
    // WARNING: DO NOT FOLLOW THIS POINTER. IT WILL BE DESTROYED.
    last_closed_connection_ = connection;
  }

  // ApplicationTestBase:
  ApplicationDelegate* GetApplicationDelegate() override { return this; }

  // Overridden from BrowserDelegate:
  void BrowserClosed(Browser* browser) override {
    scoped_ptr<Browser> browser_owner(browser);
    DCHECK_GT(browsers_.count(browser), 0u);
    browsers_.erase(browser);
  }

  bool InitUIIfNecessary(Browser* browser, mojo::View* root_view) override {
    return true;
  }

 private:
  mojo::ApplicationImpl* app_;
  void* last_closed_connection_;
  std::set<Browser*> browsers_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(BrowserTest);
};

// This test verifies that closing a Browser closes the associated application
// connection with the view manager.
TEST_F(BrowserTest, ClosingBrowserClosesAppConnection) {
  Browser* browser = CreateBrowser();
  ASSERT_NE(nullptr, browser);
  mojo::ApplicationConnection* view_manager_connection =
      browser->view_manager_init_.connection();
  ASSERT_NE(nullptr, view_manager_connection);
  BrowserClosed(browser);
  EXPECT_EQ(last_closed_connection(), view_manager_connection);
}

}  // namespace mandoline
