// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/ppapi_plugin/ppapi_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/plugin/content_plugin_client.h"
#include "crypto/nss_util.h"
#include "ppapi/proxy/proxy_module.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"
#endif

#if defined(OS_LINUX)
#include "content/public/common/sandbox_init.h"
#endif

#if defined(OS_POSIX) && !defined(OS_ANDROID)
#include <stdlib.h>
#endif

#if defined(OS_WIN)
sandbox::TargetServices* g_target_services = NULL;
#else
void* g_target_services = 0;
#endif

namespace content {

// Main function for starting the PPAPI plugin process.
int PpapiPluginMain(const MainFunctionParams& parameters) {
  const CommandLine& command_line = parameters.command_line;

#if defined(OS_WIN)
  g_target_services = parameters.sandbox_info->target_services;
#endif

  // If |g_target_services| is not null this process is sandboxed. One side
  // effect is that we can't pop dialogs like ChildProcess::WaitForDebugger()
  // does.
  if (command_line.HasSwitch(switches::kPpapiStartupDialog)) {
    if (g_target_services)
      base::debug::WaitForDebugger(2*60, false);
    else
      ChildProcess::WaitForDebugger("Ppapi");
  }

  // Set the default locale to be the current UI language. WebKit uses ICU's
  // default locale for some font settings (especially switching between
  // Japanese and Chinese fonts for the same characters).
  if (command_line.HasSwitch(switches::kLang)) {
    std::string locale = command_line.GetSwitchValueASCII(switches::kLang);
    base::i18n::SetICUDefaultLocale(locale);

#if defined(OS_POSIX) && !defined(OS_ANDROID)
    // TODO(shess): Flash appears to have a POSIX locale dependency
    // outside of the existing PPAPI ICU support.  Certain games hang
    // while loading, and it seems related to datetime formatting.
    // http://crbug.com/155396
    // http://crbug.com/155671
    //
    // ICU can accept "en-US" or "en_US", but POSIX wants "en_US".
    std::replace(locale.begin(), locale.end(), '-', '_');
    locale.append(".UTF-8");
    setlocale(LC_ALL, locale.c_str());
    setenv("LANG", locale.c_str(), 0);
#endif
  }

  MessageLoop main_message_loop;
  base::PlatformThread::SetName("CrPPAPIMain");

#if defined(OS_LINUX) && defined(USE_NSS)
  // Some out-of-process PPAPI plugins use NSS.
  // NSS must be initialized before enabling the sandbox below.
  crypto::InitNSSSafely();
#endif

  // Allow the embedder to perform any necessary per-process initialization
  // before the sandbox is initialized.
  if (GetContentClient()->plugin())
    GetContentClient()->plugin()->PreSandboxInitialization();

#if defined(OS_LINUX)
  InitializeSandbox();
#endif

  ChildProcess ppapi_process;
  ppapi_process.set_main_thread(
      new PpapiThread(parameters.command_line, false));  // Not a broker.

  main_message_loop.Run();
  return 0;
}

}  // namespace content
