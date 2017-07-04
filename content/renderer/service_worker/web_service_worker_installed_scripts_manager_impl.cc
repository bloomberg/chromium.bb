// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/web_service_worker_installed_scripts_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"

namespace content {

// static
std::unique_ptr<blink::WebServiceWorkerInstalledScriptsManager>
WebServiceWorkerInstalledScriptsManagerImpl::Create() {
  // TODO(shimazu): Pass |installed_urls| from the browser.
  std::vector<GURL> installed_urls;
  // TODO(shimazu): Create and bind a mojom interface on the io thread.
  return base::WrapUnique<WebServiceWorkerInstalledScriptsManagerImpl>(
      new WebServiceWorkerInstalledScriptsManagerImpl(
          std::move(installed_urls)));
}

WebServiceWorkerInstalledScriptsManagerImpl::
    WebServiceWorkerInstalledScriptsManagerImpl(
        std::vector<GURL>&& installed_urls)
    : installed_urls_(installed_urls.begin(), installed_urls.end()) {}

WebServiceWorkerInstalledScriptsManagerImpl::
    ~WebServiceWorkerInstalledScriptsManagerImpl() = default;

bool WebServiceWorkerInstalledScriptsManagerImpl::IsScriptInstalled(
    const blink::WebURL& web_script_url) const {
  return base::ContainsKey(installed_urls_, web_script_url);
}

std::unique_ptr<blink::WebServiceWorkerInstalledScriptsManager::RawScriptData>
WebServiceWorkerInstalledScriptsManagerImpl::GetRawScriptData(
    const blink::WebURL& web_script_url) {
  // TODO(shimazu): Implement here.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace content
