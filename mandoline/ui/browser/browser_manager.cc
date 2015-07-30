// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/browser_manager.h"

#include "base/command_line.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mandoline/ui/browser/browser.h"

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

Browser* BrowserManager::CreateBrowser(const GURL& default_url) {
  DCHECK(app_);
  Browser* browser = new Browser(app_, this, default_url);
  browsers_.insert(browser);
  return browser;
}

void BrowserManager::LaunchURL(const mojo::String& url) {
  DCHECK(!browsers_.empty());
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = url;
  // TODO(fsamuel): Create a new Browser once we support multiple browser
  // windows.
  (*browsers_.begin())->ReplaceContentWithRequest(request.Pass());
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
}

bool BrowserManager::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<LaunchHandler>(this);
  return true;
}

void BrowserManager::BrowserClosed(Browser* browser) {
  scoped_ptr<Browser> browser_owner(browser);
  DCHECK_GT(browsers_.count(browser), 0u);
  browsers_.erase(browser);
  if (browsers_.empty())
    app_->Quit();
}

void BrowserManager::InitUIIfNecessary(Browser* browser, mojo::View* view) {
  DCHECK_GT(view->viewport_metrics().device_pixel_ratio, 0);
#if defined(USE_AURA)
  if (!aura_init_)
    aura_init_.reset(new AuraInit(view, app_->shell()));
#endif
}

void BrowserManager::Create(mojo::ApplicationConnection* connection,
                            mojo::InterfaceRequest<LaunchHandler> request) {
  launch_handler_bindings_.AddBinding(this, request.Pass());
}

}  // namespace mandoline
