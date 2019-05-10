// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_delegate_impl.h"

#include <utility>

#include "base/callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_ui_delegate_impl_factory.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

// static
WebAppUiDelegateImpl* WebAppUiDelegateImpl::Get(Profile* profile) {
  return WebAppUiDelegateImplFactory::GetForProfile(profile);
}

WebAppUiDelegateImpl::WebAppUiDelegateImpl(Profile* profile)
    : profile_(profile) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    base::Optional<AppId> app_id = GetAppIdForBrowser(browser);
    if (!app_id.has_value())
      continue;

    ++num_windows_for_apps_map_[app_id.value()];
  }

  BrowserList::AddObserver(this);
  WebAppProvider::Get(profile_)->set_ui_delegate(this);
}

WebAppUiDelegateImpl::~WebAppUiDelegateImpl() = default;

void WebAppUiDelegateImpl::Shutdown() {
  WebAppProvider::Get(profile_)->set_ui_delegate(nullptr);
  BrowserList::RemoveObserver(this);
}

size_t WebAppUiDelegateImpl::GetNumWindowsForApp(const AppId& app_id) {
  auto it = num_windows_for_apps_map_.find(app_id);
  if (it == num_windows_for_apps_map_.end())
    return 0;

  return it->second;
}

void WebAppUiDelegateImpl::NotifyOnAllAppWindowsClosed(
    const AppId& app_id,
    base::OnceClosure callback) {
  const size_t num_windows_for_app = GetNumWindowsForApp(app_id);
  if (num_windows_for_app == 0) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  windows_closed_requests_map_[app_id].push_back(std::move(callback));
}

void WebAppUiDelegateImpl::OnBrowserAdded(Browser* browser) {
  base::Optional<AppId> app_id = GetAppIdForBrowser(browser);
  if (!app_id.has_value())
    return;

  ++num_windows_for_apps_map_[app_id.value()];
}

void WebAppUiDelegateImpl::OnBrowserRemoved(Browser* browser) {
  base::Optional<AppId> app_id_opt = GetAppIdForBrowser(browser);
  if (!app_id_opt.has_value())
    return;

  const auto& app_id = app_id_opt.value();

  size_t& num_windows_for_app = num_windows_for_apps_map_[app_id];
  DCHECK_GT(num_windows_for_app, 0u);
  --num_windows_for_app;

  if (num_windows_for_app > 0)
    return;

  auto it = windows_closed_requests_map_.find(app_id);
  if (it == windows_closed_requests_map_.end())
    return;

  for (auto& callback : it->second)
    std::move(callback).Run();

  windows_closed_requests_map_.erase(app_id);
}

base::Optional<AppId> WebAppUiDelegateImpl::GetAppIdForBrowser(
    Browser* browser) {
  if (browser->profile() != profile_)
    return base::nullopt;

  if (!browser->web_app_controller())
    return base::nullopt;

  return browser->web_app_controller()->GetAppId();
}

}  // namespace web_app
