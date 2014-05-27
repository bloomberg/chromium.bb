// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_browser_main_parts.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/url_constants.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_devtools_delegate.h"
#include "content/shell/browser/shell_net_log.h"
#include "content/shell/common/shell_switches.h"
#include "grit/net_resources.h"
#include "net/base/filename_util.h"
#include "net/base/net_module.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"
#include "webkit/browser/quota/quota_manager.h"

#if defined(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#include "content/shell/browser/shell_plugin_service_filter.h"
#endif

#if defined(OS_ANDROID)
#include "components/breakpad/browser/crash_dump_manager_android.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#endif

#if defined(USE_AURA) && defined(USE_X11)
#include "ui/events/x/touch_factory_x11.h"
#endif
#if !defined(OS_CHROMEOS) && defined(USE_AURA) && defined(OS_LINUX)
#include "ui/base/ime/input_method_initializer.h"
#endif

namespace content {

namespace {

// Default quota for each origin is 5MB.
const int kDefaultLayoutTestQuotaBytes = 5 * 1024 * 1024;

GURL GetStartupURL() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kContentBrowserTest))
    return GURL();
  const CommandLine::StringVector& args = command_line->GetArgs();

#if defined(OS_ANDROID)
  // Delay renderer creation on Android until surface is ready.
  return GURL();
#endif

  if (args.empty())
    return GURL("http://www.google.com/");

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(base::FilePath(args[0]));
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
    : BrowserMainParts(), parameters_(parameters), run_message_loop_(true) {}

ShellBrowserMainParts::~ShellBrowserMainParts() {
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

void ShellBrowserMainParts::PreMainMessageLoopRun() {
#if defined(OS_ANDROID)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    base::FilePath crash_dumps_dir =
        CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kCrashDumpsDir);
    crash_dump_manager_.reset(new breakpad::CrashDumpManager(crash_dumps_dir));
  }
#endif
  net_log_.reset(new ShellNetLog("content_shell"));
  browser_context_.reset(new ShellBrowserContext(false, net_log_.get()));
  off_the_record_browser_context_.reset(
      new ShellBrowserContext(true, net_log_.get()));

  Shell::Initialize();
  net::NetModule::SetResourceProvider(PlatformResourceProvider);

  devtools_delegate_.reset(new ShellDevToolsDelegate(browser_context_.get()));

  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    Shell::CreateNewWindow(browser_context_.get(),
                           GetStartupURL(),
                           NULL,
                           MSG_ROUTING_NONE,
                           gfx::Size());
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    quota::QuotaManager* quota_manager =
        BrowserContext::GetDefaultStoragePartition(browser_context())
            ->GetQuotaManager();
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&quota::QuotaManager::SetTemporaryGlobalOverrideQuota,
                   quota_manager,
                   kDefaultLayoutTestQuotaBytes *
                       quota::QuotaManager::kPerHostTemporaryPortion,
                   quota::QuotaCallback()));
#if defined(ENABLE_PLUGINS)
    PluginService* plugin_service = PluginService::GetInstance();
    plugin_service_filter_.reset(new ShellPluginServiceFilter);
    plugin_service->SetFilter(plugin_service_filter_.get());
#endif
  }

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
  if (devtools_delegate_)
    devtools_delegate_->Stop();
  browser_context_.reset();
  off_the_record_browser_context_.reset();
}

}  // namespace
