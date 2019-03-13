// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"
#include "fuchsia/runners/cast/fake_queryable_data.h"
#include "fuchsia/runners/cast/queryable_data_bindings.h"

namespace {

class QueryableDataBindingsTest
    : public cr_fuchsia::WebEngineBrowserTest,
      public chromium::web::NavigationEventObserver {
 public:
  QueryableDataBindingsTest()
      : nav_observer_binding_(this),
        queryable_data_service_binding_(&queryable_data_service_) {
    set_test_server_root(base::FilePath("fuchsia/runners/cast/testdata"));
  }

  ~QueryableDataBindingsTest() override = default;

  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    base::ScopedAllowBlockingForTesting allow_blocking;
    frame_ = WebEngineBrowserTest::CreateFrame(this);
    frame_->SetNavigationEventObserver(nav_observer_binding_.NewBinding());
  }

  void CheckLoadUrl(const std::string& url,
                    chromium::web::NavigationController* controller) {
    navigate_run_loop_ = std::make_unique<base::RunLoop>();
    controller->LoadUrl(url, chromium::web::LoadUrlParams());
    navigate_run_loop_->Run();
    navigate_run_loop_.reset();
  }

  // chromium::web::NavigationEventObserver implementation.
  void OnNavigationStateChanged(
      chromium::web::NavigationEvent change,
      OnNavigationStateChangedCallback callback) override {
    if (navigate_run_loop_)
      navigate_run_loop_->Quit();
    callback();
  }

 protected:
  std::unique_ptr<base::RunLoop> navigate_run_loop_;
  chromium::web::FramePtr frame_;
  FakeQueryableData queryable_data_service_;
  fidl::Binding<chromium::web::NavigationEventObserver> nav_observer_binding_;
  fidl::Binding<chromium::cast::QueryableData> queryable_data_service_binding_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueryableDataBindingsTest);
};

IN_PROC_BROWSER_TEST_F(QueryableDataBindingsTest, VariousTypes) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/queryable_bindings.html"));

  queryable_data_service_.Add("string", base::Value("foo"));
  queryable_data_service_.Add("number", base::Value(123));
  queryable_data_service_.Add("null", base::Value());
  base::DictionaryValue dict;
  dict.SetString("key", "val");
  queryable_data_service_.Add("dict", dict);

  QueryableDataBindings bindings(
      frame_.get(), queryable_data_service_binding_.NewBinding().Bind());

  chromium::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());
  frame_->SetJavaScriptLogLevel(chromium::web::LogLevel::INFO);
  CheckLoadUrl(test_url.spec(), controller.get());

  base::RunLoop get_visible_entry_loop;
  cr_fuchsia::ResultReceiver<std::unique_ptr<chromium::web::NavigationEntry>>
      visible_entry_promise(get_visible_entry_loop.QuitClosure());
  controller->GetVisibleEntry(cr_fuchsia::CallbackToFitFunction(
      visible_entry_promise.GetReceiveCallback()));
  get_visible_entry_loop.Run();
  EXPECT_EQ((*visible_entry_promise)->title, "foo 123 null {\"key\":\"val\"}");
}

IN_PROC_BROWSER_TEST_F(QueryableDataBindingsTest, NoValues) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/queryable_bindings.html"));

  QueryableDataBindings bindings(
      frame_.get(), queryable_data_service_binding_.NewBinding().Bind());

  chromium::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());
  frame_->SetJavaScriptLogLevel(chromium::web::LogLevel::INFO);
  CheckLoadUrl(test_url.spec(), controller.get());

  base::RunLoop get_visible_entry_loop;
  cr_fuchsia::ResultReceiver<std::unique_ptr<chromium::web::NavigationEntry>>
      visible_entry_promise(get_visible_entry_loop.QuitClosure());
  controller->GetVisibleEntry(cr_fuchsia::CallbackToFitFunction(
      visible_entry_promise.GetReceiveCallback()));
  get_visible_entry_loop.Run();
  EXPECT_EQ((*visible_entry_promise)->title, "null null null null");
}

}  // namespace
