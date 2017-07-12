// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/web_service_worker_installed_scripts_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

namespace {

// Internal lives on the IO thread. This receives mojom::ServiceWorkerScriptInfo
// for all installed scripts and then starts reading the body and meta data from
// the browser. This instance will be kept alive as long as the Mojo's
// connection is established.
class Internal : public mojom::ServiceWorkerInstalledScriptsManager {
 public:
  // Called on the IO thread.
  // Creates and binds a new Internal instance to |request|.
  static void Create(
      mojom::ServiceWorkerInstalledScriptsManagerRequest request) {
    mojo::MakeStrongBinding(base::MakeUnique<Internal>(), std::move(request));
  }

  // Implements mojom::ServiceWorkerInstalledScriptsManager.
  // Called on the IO thread.
  void TransferInstalledScript(
      mojom::ServiceWorkerScriptInfoPtr script_info) override {}
};

}  // namespace

// static
std::unique_ptr<blink::WebServiceWorkerInstalledScriptsManager>
WebServiceWorkerInstalledScriptsManagerImpl::Create(
    mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  auto installed_scripts_manager =
      base::WrapUnique<WebServiceWorkerInstalledScriptsManagerImpl>(
          new WebServiceWorkerInstalledScriptsManagerImpl(
              std::move(installed_scripts_info->installed_urls)));
  // TODO(shimazu): Implement a container class which is shared among
  // WebServiceWorkerInstalledScriptsManagerImpl and Internal to pass the data
  // between the IO thread and the worker thread.
  io_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&Internal::Create,
                     std::move(installed_scripts_info->manager_request)));
  return installed_scripts_manager;
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
