// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/browser_manager.h"

#include "mandoline/ui/browser/browser.h"

namespace mandoline {

BrowserManager::BrowserManager()
    : app_(nullptr) {
}

BrowserManager::~BrowserManager() {
  DCHECK(browsers_.empty());
}

Browser* BrowserManager::CreateBrowser() {
  DCHECK(app_);
  Browser* browser = new Browser(app_, this);
  browsers_.insert(browser);
  return browser;
}

void BrowserManager::BrowserClosed(Browser* browser) {
  scoped_ptr<Browser> browser_owner(browser);
  DCHECK_GT(browsers_.count(browser), 0u);
  browsers_.erase(browser);
  if (browsers_.empty())
    app_->Terminate();
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
#if defined(USE_AURA)
  aura_init_.reset(new AuraInit(app->shell()));
#endif
  CreateBrowser();
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
