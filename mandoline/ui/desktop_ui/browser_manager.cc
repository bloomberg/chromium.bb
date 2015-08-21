// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/desktop_ui/browser_manager.h"

#include "base/command_line.h"
#include "base/time/time.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mandoline/ui/desktop_ui/browser_window.h"
#include "mojo/services/tracing/public/cpp/switches.h"
#include "mojo/services/tracing/public/interfaces/tracing.mojom.h"

namespace mandoline {

namespace {

const char kGoogleURL[] = "http://www.google.com";

}  // namespace

BrowserManager::BrowserManager()
    : app_(nullptr) {
}

BrowserManager::~BrowserManager() {
  DCHECK(browsers_.empty());
}

BrowserWindow* BrowserManager::CreateBrowser(const GURL& default_url) {
  BrowserWindow* browser = new BrowserWindow(app_, this);
  browsers_.insert(browser);
  browser->LoadURL(default_url);
  return browser;
}

void BrowserManager::BrowserWindowClosed(BrowserWindow* browser) {
  scoped_ptr<BrowserWindow> browser_owner(browser);
  DCHECK_GT(browsers_.count(browser), 0u);
  browsers_.erase(browser);
  if (browsers_.empty())
    app_->Quit();
}

void BrowserManager::LaunchURL(const mojo::String& url) {
  DCHECK(!browsers_.empty());
  // TODO(fsamuel): Create a new Browser once we support multiple browser
  // windows.
  (*browsers_.begin())->LoadURL(GURL(url));
}

void BrowserManager::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Create a Browser for each valid URL in the command line.
  for (const auto& arg : command_line->GetArgs()) {
    GURL url(arg);
    if (url.is_valid())
      CreateBrowser(url);
  }
  // If there were no valid URLs in the command line create a Browser with the
  // default URL.
  if (browsers_.empty())
    CreateBrowser(GURL(kGoogleURL));

  // Record the browser startup time metrics for performance testing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          tracing::kEnableStatsCollectionBindings)) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:tracing");
    tracing::StartupPerformanceDataCollectorPtr collector;
    app_->ConnectToService(request.Pass(), &collector);
    // TODO(msw): When to record the browser message loop start time?
    const base::Time startup_time = base::Time::Now();
    collector->SetBrowserMessageLoopStartTime(startup_time.ToInternalValue());
    // TODO(msw): When to record the browser window display time?
    const base::Time display_time = base::Time::Now();
    collector->SetBrowserWindowDisplayTime(display_time.ToInternalValue());
    // TODO(msw): When to record the browser open tabs time?
    const base::Time open_tabs_time = base::Time::Now();
    collector->SetBrowserOpenTabsTime(open_tabs_time.ToInternalValue());
  }
}

bool BrowserManager::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<LaunchHandler>(this);
  return true;
}

void BrowserManager::Create(mojo::ApplicationConnection* connection,
                            mojo::InterfaceRequest<LaunchHandler> request) {
  launch_handler_bindings_.AddBinding(this, request.Pass());
}

}  // namespace mandoline
