// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/test_runner/test_runner_application_delegate.h"

#include <iostream>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/mus/public/cpp/scoped_window_ptr.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_host_factory.h"
#include "components/test_runner/blink_test_platform_support.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/shell.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#endif

namespace web_view {

TestRunnerApplicationDelegate::TestRunnerApplicationDelegate()
    : shell_(nullptr), root_(nullptr), content_(nullptr) {}

TestRunnerApplicationDelegate::~TestRunnerApplicationDelegate() {
  if (root_)
    mus::ScopedWindowPtr::DeleteWindowOrWindowManager(root_);
}

void TestRunnerApplicationDelegate::LaunchURL(const GURL& test_url) {
  if (!web_view_) {
    web_view_.reset(new WebView(this));
    web_view_->Init(shell_, content_);
  }
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = test_url.spec();
  web_view_->web_view()->LoadRequest(std::move(request));
}

void TestRunnerApplicationDelegate::Terminate() {
  if (root_)
    mus::ScopedWindowPtr::DeleteWindowOrWindowManager(root_);
}

////////////////////////////////////////////////////////////////////////////////
// mojo::ShellClient implementation:

void TestRunnerApplicationDelegate::Initialize(mojo::Shell* shell,
                                               const std::string& url,
                                               uint32_t id) {
  if (!test_runner::BlinkTestPlatformInitialize()) {
    NOTREACHED() << "Test environment could not be properly set up for blink.";
  }
  shell_ = shell;
  mus::CreateWindowTreeHost(shell_, this, &host_, nullptr);
}

bool TestRunnerApplicationDelegate::AcceptConnection(
    mojo::Connection* connection) {
  connection->AddInterface<web_view::LayoutTestRunner>(this);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// mus::WindowTreeDelegate implementation:

void TestRunnerApplicationDelegate::OnEmbed(mus::Window* root) {
  root_ = root;

  // If this is a sys-check, then terminate in the next cycle.
  const char kCheckLayoutTestSysDeps[] = "check-layout-test-sys-deps";
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kCheckLayoutTestSysDeps)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&TestRunnerApplicationDelegate::Terminate,
                              base::Unretained(this)));
    return;
  }

  const gfx::Size kViewportSize(800, 600);
  host_->SetSize(mojo::Size::From(kViewportSize));

  content_ = root_->connection()->NewWindow();
  root_->AddChild(content_);
  content_->SetBounds(gfx::Rect(kViewportSize));
  content_->SetVisible(true);

  std::cout << "#READY\n";
  std::cout.flush();

  auto cmdline_args = base::CommandLine::ForCurrentProcess()->GetArgs();
  test_extractor_.reset(new test_runner::TestInfoExtractor(cmdline_args));

  scoped_ptr<test_runner::TestInfo> test_info = test_extractor_->GetNextTest();
  if (test_info)
    LaunchURL(test_info->url);
}

void TestRunnerApplicationDelegate::OnConnectionLost(
    mus::WindowTreeConnection* connection) {
  root_ = nullptr;
  shell_->Quit();
}

////////////////////////////////////////////////////////////////////////////////
// mojom::WebViewClient implementation:

void TestRunnerApplicationDelegate::TopLevelNavigateRequest(
    mojo::URLRequestPtr request) {
  web_view_->web_view()->LoadRequest(std::move(request));
}

void TestRunnerApplicationDelegate::TopLevelNavigationStarted(
    const mojo::String& url) {}
void TestRunnerApplicationDelegate::LoadingStateChanged(bool is_loading,
                                                        double progress) {}
void TestRunnerApplicationDelegate::BackForwardChanged(
    mojom::ButtonState back_button,
    mojom::ButtonState forward_button) {}
void TestRunnerApplicationDelegate::TitleChanged(const mojo::String& title) {}

////////////////////////////////////////////////////////////////////////////////
// LayoutTestRunner implementation:

void TestRunnerApplicationDelegate::TestFinished() {
  std::cout << "#EOF\n";
  std::cout.flush();

  std::cerr << "#EOF\n";
  std::cerr.flush();

  scoped_ptr<test_runner::TestInfo> test_info = test_extractor_->GetNextTest();
  if (test_info)
    LaunchURL(test_info->url);
  else
    Terminate();
}

////////////////////////////////////////////////////////////////////////////////
// mojo::InterfaceFactory<LayoutTestRunner> implementation:

void TestRunnerApplicationDelegate::Create(
    mojo::Connection* connection,
    mojo::InterfaceRequest<web_view::LayoutTestRunner> request) {
  layout_test_runner_.AddBinding(this, std::move(request));
}

}  // namespace web_view
