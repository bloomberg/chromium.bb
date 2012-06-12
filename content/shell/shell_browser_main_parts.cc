// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_browser_main_parts.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/common/content_switches.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "content/shell/shell_devtools_delegate.h"
#include "content/shell/shell_switches.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_module.h"

#if defined(OS_ANDROID)
#include "net/base/network_change_notifier.h"
#include "net/android/network_change_notifier_factory.h"
#endif

namespace content {

static GURL GetStartupURL() {
  const CommandLine::StringVector& args =
      CommandLine::ForCurrentProcess()->GetArgs();
  if (args.empty())
    return GURL("http://www.google.com/");

  return GURL(args[0]);
}

ShellBrowserMainParts::ShellBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts(),
      devtools_delegate_(NULL) {
}

ShellBrowserMainParts::~ShellBrowserMainParts() {
}

#if !defined(OS_MACOSX)
void ShellBrowserMainParts::PreMainMessageLoopStart() {
}
#endif

void ShellBrowserMainParts::PostMainMessageLoopStart() {
#if defined(OS_ANDROID)
  MessageLoopForUI::current()->Start();
#endif
}

void ShellBrowserMainParts::PreEarlyInitialization() {
#if defined(OS_ANDROID)
  net::NetworkChangeNotifier::SetFactory(
      new net::android::NetworkChangeNotifierFactory());
#endif
}

void ShellBrowserMainParts::PreMainMessageLoopRun() {
  browser_context_.reset(new ShellBrowserContext);

  Shell::PlatformInitialize();
  net::NetModule::SetResourceProvider(Shell::PlatformResourceProvider);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    std::string port_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    int port;
    if (base::StringToInt(port_str, &port) && port > 0 && port < 65535) {
      devtools_delegate_ = new ShellDevToolsDelegate(
          port,
          browser_context_->GetRequestContext());
    } else {
      DLOG(WARNING) << "Invalid http debugger port number " << port;
    }
  }

  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    Shell::CreateNewWindow(browser_context_.get(),
                           GetStartupURL(),
                           NULL,
                           MSG_ROUTING_NONE,
                           NULL);
  }
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  if (devtools_delegate_)
    devtools_delegate_->Stop();
  browser_context_.reset();
}

}  // namespace
