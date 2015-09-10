// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/test_runner/test_runner_application_delegate.h"

#include <iostream>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "components/view_manager/public/cpp/scoped_view_ptr.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_tree_connection.h"
#include "components/view_manager/public/cpp/view_tree_host_factory.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "net/base/filename_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#endif

namespace web_view {

namespace {

GURL GetURLForLayoutTest(const std::string& test_name,
                         base::FilePath* current_working_directory,
                         bool* enable_pixel_dumping,
                         std::string* expected_pixel_hash) {
  // A test name is formated like file:///path/to/test'--pixel-test'pixelhash
  std::string path_or_url = test_name;
  std::string pixel_switch;
  std::string pixel_hash;
  std::string::size_type separator_position = path_or_url.find('\'');
  if (separator_position != std::string::npos) {
    pixel_switch = path_or_url.substr(separator_position + 1);
    path_or_url.erase(separator_position);
  }
  separator_position = pixel_switch.find('\'');
  if (separator_position != std::string::npos) {
    pixel_hash = pixel_switch.substr(separator_position + 1);
    pixel_switch.erase(separator_position);
  }
  if (enable_pixel_dumping) {
    *enable_pixel_dumping =
        (pixel_switch == "--pixel-test" || pixel_switch == "-p");
  }
  if (expected_pixel_hash)
    *expected_pixel_hash = pixel_hash;

  GURL test_url(path_or_url);
  if (!(test_url.is_valid() && test_url.has_scheme())) {
    // This is a test.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
#if defined(OS_WIN)
    base::FilePath::StringType wide_path_or_url =
        base::SysNativeMBToWide(path_or_url);
    base::FilePath local_file(wide_path_or_url);
#else
    base::FilePath local_file(path_or_url);
#endif
    if (!base::PathExists(local_file)) {
      base::FilePath base_path;
      PathService::Get(base::DIR_SOURCE_ROOT, &base_path);
      local_file = base_path.Append(FILE_PATH_LITERAL("third_party"))
                       .Append(FILE_PATH_LITERAL("WebKit"))
                       .Append(FILE_PATH_LITERAL("LayoutTests"))
                       .Append(local_file);
    }
    test_url = net::FilePathToFileURL(base::MakeAbsoluteFilePath(local_file));
  }
  base::FilePath local_path;
  if (current_working_directory) {
    // We're outside of the message loop here, and this is a test.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (net::FileURLToFilePath(test_url, &local_path))
      *current_working_directory = local_path.DirName();
    else
      base::GetCurrentDirectory(current_working_directory);
  }
  return test_url;
}

bool GetNextTestURL(const base::CommandLine::StringVector& args,
                    size_t* position,
                    GURL* test_url) {
  std::string test_string;
  if (*position >= args.size())
    return false;
  if (args[*position] == FILE_PATH_LITERAL("-")) {
    do {
      bool success = !!std::getline(std::cin, test_string, '\n');
      if (!success)
        return false;
    } while (test_string.empty());
  } else {
#if defined(OS_WIN)
    test_string = base::WideToUTF8(args[(*position)++]);
#else
    test_string = args[(*position)++];
#endif
  }

  DCHECK(!test_string.empty());
  if (test_string == "QUIT")
    return false;
  bool enable_pixel_dumps;
  std::string pixel_hash;
  base::FilePath cwd;
  *test_url =
      GetURLForLayoutTest(test_string, &cwd, &enable_pixel_dumps, &pixel_hash);
  return true;
}

}  // namespace

TestRunnerApplicationDelegate::TestRunnerApplicationDelegate()
    : app_(nullptr),
      root_(nullptr),
      content_(nullptr),
      cmdline_position_(0u) {}

TestRunnerApplicationDelegate::~TestRunnerApplicationDelegate() {
  if (root_)
    mojo::ScopedViewPtr::DeleteViewOrViewManager(root_);
}

void TestRunnerApplicationDelegate::LaunchURL(const GURL& test_url) {
  if (!web_view_) {
    content_->SetAccessPolicy(mojo::ViewTree::ACCESS_POLICY_EMBED_ROOT);
    web_view_.reset(new WebView(this));
    web_view_->Init(app_, content_);
  }
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = test_url.spec();
  web_view_->web_view()->LoadRequest(request.Pass());
}

void TestRunnerApplicationDelegate::Terminate() {
  if (root_)
    mojo::ScopedViewPtr::DeleteViewOrViewManager(root_);
}

////////////////////////////////////////////////////////////////////////////////
// mojo::ApplicationDelegate implementation:

void TestRunnerApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
  mojo::CreateSingleViewTreeHost(app_, this, &host_);
}

bool TestRunnerApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<web_view::LayoutTestRunner>(this);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// mojo::ViewTreeDelegate implementation:

void TestRunnerApplicationDelegate::OnEmbed(mojo::View* root) {
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

  content_ = root_->connection()->CreateView();
  root_->AddChild(content_);
  content_->SetBounds(*mojo::Rect::From(gfx::Rect(kViewportSize)));
  content_->SetVisible(true);

  cmdline_args_ = base::CommandLine::ForCurrentProcess()->GetArgs();
  std::cout << "#READY\n";
  std::cout.flush();

  cmdline_position_ = 0;
  GURL test_url;
  if (GetNextTestURL(cmdline_args_, &cmdline_position_, &test_url))
    LaunchURL(test_url);
}

void TestRunnerApplicationDelegate::OnConnectionLost(
    mojo::ViewTreeConnection* connection) {
  root_ = nullptr;
  app_->Quit();
}

////////////////////////////////////////////////////////////////////////////////
// mojom::WebViewClient implementation:

void TestRunnerApplicationDelegate::TopLevelNavigate(
    mojo::URLRequestPtr request) {
  web_view_->web_view()->LoadRequest(request.Pass());
}

void TestRunnerApplicationDelegate::LoadingStateChanged(bool is_loading) {}
void TestRunnerApplicationDelegate::ProgressChanged(double progress) {}
void TestRunnerApplicationDelegate::TitleChanged(const mojo::String& title) {}

////////////////////////////////////////////////////////////////////////////////
// LayoutTestRunner implementation:

void TestRunnerApplicationDelegate::TestFinished() {
  std::cout << "#EOF\n";
  std::cout.flush();

  std::cerr << "#EOF\n";
  std::cerr.flush();

  GURL test_url;
  if (GetNextTestURL(cmdline_args_, &cmdline_position_, &test_url))
    LaunchURL(test_url);
  else
    Terminate();
}

////////////////////////////////////////////////////////////////////////////////
// mojo::InterfaceFactory<LayoutTestRunner> implementation:

void TestRunnerApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<web_view::LayoutTestRunner> request) {
  layout_test_runner_.AddBinding(this, request.Pass());
}

}  // namespace web_view
