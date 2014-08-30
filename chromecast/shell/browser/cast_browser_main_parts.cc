// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/browser/cast_browser_main_parts.h"

#include "base/command_line.h"
#include "base/prefs/pref_registry_simple.h"
#include "chromecast/common/chromecast_config.h"
#include "chromecast/metrics/cast_metrics_service_client.h"
#include "chromecast/net/network_change_notifier_cast.h"
#include "chromecast/net/network_change_notifier_factory_cast.h"
#include "chromecast/service/cast_service.h"
#include "chromecast/shell/browser/cast_browser_context.h"
#include "chromecast/shell/browser/cast_browser_process.h"
#include "chromecast/shell/browser/devtools/remote_debugging_server.h"
#include "chromecast/shell/browser/url_request_context_factory.h"
#include "chromecast/shell/browser/webui/webui_cast.h"
#include "content/public/common/content_switches.h"

namespace chromecast {
namespace shell {

namespace {

struct DefaultCommandLineSwitch {
  const char* const switch_name;
  const char* const switch_value;
};

DefaultCommandLineSwitch g_default_switches[] = {
  { switches::kDisableApplicationCache, "" },
  { switches::kDisablePlugins, "" },
  { NULL, NULL },  // Termination
};

void AddDefaultCommandLineSwitches(CommandLine* command_line) {
  int i = 0;
  while (g_default_switches[i].switch_name != NULL) {
    command_line->AppendSwitchASCII(
        std::string(g_default_switches[i].switch_name),
        std::string(g_default_switches[i].switch_value));
    ++i;
  }
}

}  // namespace

CastBrowserMainParts::CastBrowserMainParts(
    const content::MainFunctionParams& parameters,
    URLRequestContextFactory* url_request_context_factory)
    : BrowserMainParts(),
      cast_browser_process_(new CastBrowserProcess()),
      url_request_context_factory_(url_request_context_factory) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  AddDefaultCommandLineSwitches(command_line);
}

CastBrowserMainParts::~CastBrowserMainParts() {
}

void CastBrowserMainParts::PreMainMessageLoopStart() {
  net::NetworkChangeNotifier::SetFactory(
      new NetworkChangeNotifierFactoryCast());
}

void CastBrowserMainParts::PostMainMessageLoopStart() {
  NOTIMPLEMENTED();
}

int CastBrowserMainParts::PreCreateThreads() {
  ChromecastConfig::Create(new PrefRegistrySimple());
  return 0;
}

void CastBrowserMainParts::PreMainMessageLoopRun() {
  url_request_context_factory_->InitializeOnUIThread();

  cast_browser_process_->SetBrowserContext(
      new CastBrowserContext(url_request_context_factory_));
  cast_browser_process_->SetMetricsServiceClient(
      metrics::CastMetricsServiceClient::Create(
          ChromecastConfig::GetInstance()->pref_service(),
          cast_browser_process_->browser_context()->GetRequestContext()));
  cast_browser_process_->SetRemoteDebuggingServer(new RemoteDebuggingServer());

  InitializeWebUI();

  cast_browser_process_->SetCastService(
      CastService::Create(cast_browser_process_->browser_context()));
  cast_browser_process_->cast_service()->Start();
}

bool CastBrowserMainParts::MainMessageLoopRun(int* result_code) {
  base::MessageLoopForUI::current()->Run();
  return true;
}

void CastBrowserMainParts::PostMainMessageLoopRun() {
  cast_browser_process_->cast_service()->Stop();
  cast_browser_process_.reset();
}

}  // namespace shell
}  // namespace chromecast
