// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/system_handler.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/settings_utils.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if defined(OS_WIN)
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/chrome_constants.h"
#endif

namespace settings {

SystemHandler::SystemHandler() {}

SystemHandler::~SystemHandler() {}

// static
void SystemHandler::AddLoadTimeData(content::WebUIDataSource* data_source) {
  data_source->AddBoolean("hardwareAccelerationEnabledAtStartup",
      g_browser_process->gpu_mode_manager()->initial_gpu_mode_pref());
}

void SystemHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("changeProxySettings",
      base::Bind(&SystemHandler::HandleChangeProxySettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("restartBrowser",
      base::Bind(&SystemHandler::HandleRestartBrowser,
                 base::Unretained(this)));
}

void SystemHandler::HandleChangeProxySettings(const base::ListValue* /*args*/) {
  base::RecordAction(base::UserMetricsAction("Options_ShowProxySettings"));
  settings_utils::ShowNetworkProxySettings(web_ui()->GetWebContents());
}

void SystemHandler::HandleRestartBrowser(const base::ListValue* /*args*/) {
#if defined(OS_WIN)
  // On Windows Breakpad will upload crash reports if the breakpad pipe name
  // environment variable is defined. So we undefine this environment variable
  // before restarting, as the restarted processes will inherit their
  // environment variables from ours, thus suppressing crash uploads.
  if (!ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled()) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (exe_module) {
      typedef void (__cdecl *ClearBreakpadPipeEnvVar)();
      ClearBreakpadPipeEnvVar clear = reinterpret_cast<ClearBreakpadPipeEnvVar>(
          GetProcAddress(exe_module, "ClearBreakpadPipeEnvironmentVariable"));
      if (clear)
        clear();
    }
  }
#endif

  chrome::AttemptRestart();
}

}  // namespace settings
