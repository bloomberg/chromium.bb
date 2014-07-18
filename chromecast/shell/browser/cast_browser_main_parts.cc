// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/browser/cast_browser_main_parts.h"

#include "base/command_line.h"
#include "chromecast/net/network_change_notifier_cast.h"
#include "chromecast/net/network_change_notifier_factory_cast.h"
#include "chromecast/service/cast_service.h"
#include "chromecast/shell/browser/cast_browser_context.h"
#include "chromecast/shell/browser/url_request_context_factory.h"

namespace chromecast {
namespace shell {

namespace {

struct DefaultCommandLineSwitch {
  const char* const switch_name;
  const char* const switch_value;
};

DefaultCommandLineSwitch g_default_switches[] = {
  { "enable-webrtc-hw-decoding", "" },
  { "disable-plugins", "" },
  { "enable-threaded-compositing", "" },
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
  return 0;
}

void CastBrowserMainParts::PreMainMessageLoopRun() {
  url_request_context_factory_->InitializeOnUIThread();

  browser_context_.reset(new CastBrowserContext(url_request_context_factory_));

  cast_service_.reset(CastService::Create(browser_context_.get()));
  cast_service_->Start();
}

bool CastBrowserMainParts::MainMessageLoopRun(int* result_code) {
  base::MessageLoopForUI::current()->Run();
  return true;
}

void CastBrowserMainParts::PostMainMessageLoopRun() {
  cast_service_->Stop();
  browser_context_.reset();
}

}  // namespace shell
}  // namespace chromecast
