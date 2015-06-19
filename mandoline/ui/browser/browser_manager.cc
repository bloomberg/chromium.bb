// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/browser_manager.h"

#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mandoline/ui/browser/browser.h"

namespace mandoline {

// TODO(sky): make ViewManager not do anything until device_pixel_ratio is
// determined. At which point this can be nuked.
class BrowserManager::DevicePixelRatioWaiter : mojo::ViewObserver {
 public:
  DevicePixelRatioWaiter(BrowserManager* manager,
                         Browser* browser,
                         mojo::View* view)
      : manager_(manager), browser_(browser), view_(view) {
    view_->AddObserver(this);
  }
  ~DevicePixelRatioWaiter() override { RemoveObserver(); }

 private:
  void RemoveObserver() {
    if (!view_)
      return;
    view_->RemoveObserver(this);
    view_ = nullptr;
  }

  // mojo::ViewObserver:
  void OnViewViewportMetricsChanged(
      mojo::View* view,
      const mojo::ViewportMetrics& old_metrics,
      const mojo::ViewportMetrics& new_metrics) override {
    if (new_metrics.device_pixel_ratio == 0)
      return;

    RemoveObserver();
    manager_->OnDevicePixelRatioAvailable(browser_, view);
  }
  void OnViewDestroyed(mojo::View* view) override { RemoveObserver(); }

  BrowserManager* manager_;
  Browser* browser_;
  mojo::View* view_;

  DISALLOW_COPY_AND_ASSIGN(DevicePixelRatioWaiter);
};

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

bool BrowserManager::InitUIIfNecessary(Browser* browser, mojo::View* view) {
  if (view->viewport_metrics().device_pixel_ratio > 0) {
#if defined(USE_AURA)
    if (!aura_init_)
      aura_init_.reset(new AuraInit(view, app_->shell()));
#endif
    return true;
  }
  DCHECK(!device_pixel_ratio_waiter_.get());
  device_pixel_ratio_waiter_.reset(
      new DevicePixelRatioWaiter(this, browser, view));
  return false;
}

void BrowserManager::OnDevicePixelRatioAvailable(Browser* browser,
                                                 mojo::View* view) {
  device_pixel_ratio_waiter_.reset();
#if defined(USE_AURA)
  DCHECK(!aura_init_.get());
  aura_init_.reset(new AuraInit(view, app_->shell()));
#endif
  browser->OnDevicePixelRatioAvailable();
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
