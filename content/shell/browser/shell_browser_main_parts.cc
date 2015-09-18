// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_browser_main_parts.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "components/devtools_http_handler/devtools_http_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/url_constants.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "content/shell/browser/shell_net_log.h"
#include "content/shell/common/shell_switches.h"
#include "net/base/filename_util.h"
#include "net/base/net_module.h"
#include "net/grit/net_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "components/crash/content/browser/crash_dump_manager_android.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#endif

#if defined(USE_AURA) && defined(USE_X11)
#include "ui/events/devices/x11/touch_factory_x11.h"
#endif
#if !defined(OS_CHROMEOS) && defined(USE_AURA) && defined(OS_LINUX)
#include "ui/base/ime/input_method_initializer.h"
#endif

namespace content {

namespace {

GURL GetStartupURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kContentBrowserTest))
    return GURL();
  const base::CommandLine::StringVector& args = command_line->GetArgs();

#if defined(OS_ANDROID)
  // Delay renderer creation on Android until surface is ready.
  return GURL();
#endif

  if (args.empty())
    return GURL("https://www.google.com/");

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(
      base::MakeAbsoluteFilePath(base::FilePath(args[0])));
}

base::StringPiece PlatformResourceProvider(int key) {
  if (key == IDR_DIR_HEADER_HTML) {
    base::StringPiece html_data =
        ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_DIR_HEADER_HTML);
    return html_data;
  }
  return base::StringPiece();
}

}  // namespace

ShellBrowserMainParts::ShellBrowserMainParts(
    const MainFunctionParams& parameters)
    : parameters_(parameters),
      run_message_loop_(true),
      devtools_http_handler_(nullptr) {
}

ShellBrowserMainParts::~ShellBrowserMainParts() {
  DCHECK(!devtools_http_handler_);
}

#if !defined(OS_MACOSX)
void ShellBrowserMainParts::PreMainMessageLoopStart() {
#if defined(USE_AURA) && defined(USE_X11)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif
}
#endif

void ShellBrowserMainParts::PostMainMessageLoopStart() {
#if defined(OS_ANDROID)
  base::MessageLoopForUI::current()->Start();
#endif
}

void ShellBrowserMainParts::PreEarlyInitialization() {
#if !defined(OS_CHROMEOS) && defined(USE_AURA) && defined(OS_LINUX)
  ui::InitializeInputMethodForTesting();
#endif
#if defined(OS_ANDROID)
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());
#endif
}

void ShellBrowserMainParts::InitializeBrowserContexts() {
  set_browser_context(new ShellBrowserContext(false, net_log_.get()));
  set_off_the_record_browser_context(
      new ShellBrowserContext(true, net_log_.get()));
}

void ShellBrowserMainParts::InitializeMessageLoopContext() {
  Shell::CreateNewWindow(browser_context_.get(),
                         GetStartupURL(),
                         NULL,
                         gfx::Size());
}

void ShellBrowserMainParts::PreMainMessageLoopRun() {
#if defined(OS_ANDROID)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    base::FilePath crash_dumps_dir =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kCrashDumpsDir);
    crash_dump_manager_.reset(new breakpad::CrashDumpManager(crash_dumps_dir));
  }
#endif

  net_log_.reset(new ShellNetLog("content_shell"));
  InitializeBrowserContexts();
  Shell::Initialize();
  net::NetModule::SetResourceProvider(PlatformResourceProvider);

  devtools_http_handler_.reset(
      ShellDevToolsManagerDelegate::CreateHttpHandler(browser_context_.get()));

  InitializeMessageLoopContext();

  if (parameters_.ui_task) {
    parameters_.ui_task->Run();
    delete parameters_.ui_task;
    run_message_loop_ = false;
  }
}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code)  {
  return !run_message_loop_;
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  devtools_http_handler_.reset();
  browser_context_.reset();
  off_the_record_browser_context_.reset();
}

}  // namespace
