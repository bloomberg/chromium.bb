// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW__TEST_RUNNER_TEST_RUNNER_APPLICATION_DELEGATE_H_
#define COMPONENTS_WEB_VIEW__TEST_RUNNER_TEST_RUNNER_APPLICATION_DELEGATE_H_

#include <stdint.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/test_runner/test_info_extractor.h"
#include "components/web_view/public/cpp/web_view.h"
#include "components/web_view/public/interfaces/web_view.mojom.h"
#include "components/web_view/test_runner/public/interfaces/layout_test_runner.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"

class GURL;

namespace web_view {

class TestRunnerApplicationDelegate
    : public mojo::ApplicationDelegate,
      public mus::WindowTreeDelegate,
      public mojom::WebViewClient,
      public LayoutTestRunner,
      public mojo::InterfaceFactory<LayoutTestRunner> {
 public:
  TestRunnerApplicationDelegate();
  ~TestRunnerApplicationDelegate() override;

 private:
  void LaunchURL(const GURL& test_url);
  void Terminate();

  // mojo::ApplicationDelegate:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override;

  // mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;

  // mojom::WebViewClient:
  void TopLevelNavigateRequest(mojo::URLRequestPtr request) override;
  void TopLevelNavigationStarted(const mojo::String& url) override;
  void LoadingStateChanged(bool is_loading, double progress) override;
  void BackForwardChanged(mojom::ButtonState back_button,
                          mojom::ButtonState forward_button) override;
  void TitleChanged(const mojo::String& title) override;
  void FindInPageMatchCountUpdated(int32_t request_id,
                                   int32_t count,
                                   bool final_update) override {}
  void FindInPageSelectionUpdated(int32_t request_id,
                                  int32_t active_match_ordinal) override {}

  // LayoutTestRunner:
  void TestFinished() override;

  // mojo::InterfaceFactory<LayoutTestRunner>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<LayoutTestRunner> request) override;

  mojo::Shell* shell_;
  mus::mojom::WindowTreeHostPtr host_;

  mus::Window* root_;
  mus::Window* content_;
  scoped_ptr<WebView> web_view_;

  scoped_ptr<test_runner::TestInfoExtractor> test_extractor_;

  mojo::WeakBindingSet<LayoutTestRunner> layout_test_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestRunnerApplicationDelegate);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW__TEST_RUNNER_TEST_RUNNER_APPLICATION_DELEGATE_H_
