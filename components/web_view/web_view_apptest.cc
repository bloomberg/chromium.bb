// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/public/cpp/web_view.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "components/mus/public/cpp/scoped_view_ptr.h"
#include "components/mus/public/cpp/tests/view_manager_test_base.h"
#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "mojo/util/filename_util.h"
#include "url/gurl.h"

namespace web_view {

class WebViewTest : public mojo::ViewManagerTestBase,
                    public mojom::WebViewClient {
 public:
  WebViewTest() : web_view_(this) {}
  ~WebViewTest() override {}

  mojom::WebView* web_view() { return web_view_.web_view(); }

  const std::string& last_title() { return last_title_; }

  void StartNestedRunLoopUntilLoadingDone() {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }

 private:
  void QuitNestedRunLoop() {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  // Overridden from ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {
    ViewManagerTestBase::Initialize(app);
    app_ = app;
  }

  // Overridden from ViewTreeDelegate:
  void OnEmbed(mojo::View* root) override {
    content_ = root->connection()->CreateView();
    root->AddChild(content_);
    content_->SetVisible(true);

    web_view_.Init(app_, content_);

    ViewManagerTestBase::OnEmbed(root);
  }

  void TearDown() override {
    mojo::ScopedViewPtr::DeleteViewOrViewManager(window_manager()->GetRoot());
    ViewManagerTestBase::TearDown();
  }

  // Overridden from web_view::mojom::WebViewClient:
  void TopLevelNavigate(mojo::URLRequestPtr request) override {}
  void LoadingStateChanged(bool is_loading) override {
    if (is_loading == false)
      QuitNestedRunLoop();
  }
  void ProgressChanged(double progress) override {}
  void TitleChanged(const mojo::String& title) override {
    last_title_ = title.get();
  }

  mojo::ApplicationImpl* app_;

  mojo::View* content_;

  web_view::WebView web_view_;

  scoped_ptr<base::RunLoop> run_loop_;

  std::string last_title_;

  DISALLOW_COPY_AND_ASSIGN(WebViewTest);
};

TEST_F(WebViewTest, TestTitleChanged) {
  base::FilePath data_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_file));
  data_file = data_file.AppendASCII("components").
              AppendASCII("test").
              AppendASCII("data").
              AppendASCII("web_view").
              AppendASCII("test_title_changed.html");
  ASSERT_TRUE(base::PathExists(data_file));

  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::util::FilePathToFileURL(data_file).spec();
  web_view()->LoadRequest(request.Pass());

  // Build a nested run loop.
  StartNestedRunLoopUntilLoadingDone();

  // Our title should have been set on the final.
  EXPECT_EQ("Test Title Changed", last_title());
}

}  // namespace web_view
