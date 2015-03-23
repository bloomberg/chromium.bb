// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_debug_ui_manager.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {
enum State {
  NOT_CALLED,
  PROCEED,
  DONT_PROCEED,
};
}  // namespace

class TestDataReductionProxyDebugUIManager
    : public DataReductionProxyDebugUIManager {
 public:
  TestDataReductionProxyDebugUIManager(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const std::string& app_locale)
      : DataReductionProxyDebugUIManager(
          ui_task_runner, io_task_runner, app_locale) {
  }

  bool IsTabClosed(const BypassResource& resource) const override {
    return is_tab_closed_return_value_;
  }

  void ShowBlockingPage(const BypassResource& resource) override {
    std::vector<BypassResource> resources;
    resources.push_back(resource);
    OnBlockingPageDone(resources, true);
  }

  bool is_tab_closed_return_value_;

 private:
  ~TestDataReductionProxyDebugUIManager() override {
  }

  DISALLOW_COPY_AND_ASSIGN(TestDataReductionProxyDebugUIManager);
};

class DataReductionProxyDebugUIManagerTest
    : public testing::Test {
 public:
  DataReductionProxyDebugUIManagerTest()
      : state_(NOT_CALLED),
        request_(context_.CreateRequest(GURL(), net::IDLE, &delegate_)) {
    ui_manager_ = new TestDataReductionProxyDebugUIManager(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::UI),
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::UI),
        "en-US");
  }

  void OnBlockingPageDoneCallback(bool proceed) {
    if (proceed)
      state_ = PROCEED;
    else
      state_ = DONT_PROCEED;
  }

  void ResetState() {
    state_ = NOT_CALLED;
  }

 protected:
  State state_;

  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;

  scoped_ptr<net::URLRequest> request_;
  scoped_refptr<TestDataReductionProxyDebugUIManager> ui_manager_;
};

// Tests that the page is loaded after selecting continue on the blocking page
// and within the five minute window when the warning is not displayed again.
TEST_F(DataReductionProxyDebugUIManagerTest, DisplayBlockingPage) {
  DataReductionProxyDebugUIManager::BypassResource resource;
  resource.is_subresource = false;
  resource.callback = base::Bind(
      &DataReductionProxyDebugUIManagerTest::OnBlockingPageDoneCallback,
      base::Unretained(this));
  resource.render_process_host_id = 0;
  resource.render_view_id = 0;

  ui_manager_->is_tab_closed_return_value_ = false;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DataReductionProxyDebugUIManager::DisplayBlockingPage,
                 ui_manager_, resource));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PROCEED, state_);

  // The warning was ignored so for the next five minutes automatically continue
  // to the page.
  ResetState();

  // This value does not matter since it will not be reached. If it is reached
  // the state will be DONT_PROCEED.
  ui_manager_->is_tab_closed_return_value_ = false;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DataReductionProxyDebugUIManager::DisplayBlockingPage,
                 ui_manager_, resource));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PROCEED, state_);
}

// Tests that the blocking page is not shown when the tab is already closed.
TEST_F(DataReductionProxyDebugUIManagerTest, DontDisplayBlockingPage) {
  DataReductionProxyDebugUIManager::BypassResource resource;
  resource.is_subresource = false;
  resource.callback = base::Bind(
      &DataReductionProxyDebugUIManagerTest::OnBlockingPageDoneCallback,
      base::Unretained(this));
  resource.render_process_host_id = 0;
  resource.render_view_id = 0;

  ui_manager_->is_tab_closed_return_value_ = true;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DataReductionProxyDebugUIManager::DisplayBlockingPage,
                 ui_manager_, resource));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DONT_PROCEED, state_);
}

// Tests that OnBlockingPageDone calls the callback of the resource.
TEST_F(DataReductionProxyDebugUIManagerTest, OnBlockingPageDone) {
  DataReductionProxyDebugUIManager::BypassResource resource;
    resource.is_subresource = false;
    resource.callback = base::Bind(
        &DataReductionProxyDebugUIManagerTest::OnBlockingPageDoneCallback,
        base::Unretained(this));
    resource.render_process_host_id = 0;
    resource.render_view_id = 0;
    std::vector<DataReductionProxyDebugUIManager::BypassResource> resources;
    resources.push_back(resource);

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&DataReductionProxyDebugUIManager::OnBlockingPageDone,
                   ui_manager_, resources, false));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(DONT_PROCEED, state_);

    ResetState();
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&DataReductionProxyDebugUIManager::OnBlockingPageDone,
                   ui_manager_, resources, true));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(PROCEED, state_);
}

}  // namespace data_reduction_proxy
