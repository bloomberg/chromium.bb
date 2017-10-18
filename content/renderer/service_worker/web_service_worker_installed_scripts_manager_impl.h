// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_INSTALLED_SCRIPTS_MANAGER_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_INSTALLED_SCRIPTS_MANAGER_IMPL_H_

#include <set>
#include <vector>

#include "content/common/service_worker/service_worker_installed_scripts_manager.mojom.h"
#include "content/renderer/service_worker/thread_safe_script_container.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerInstalledScriptsManager.h"

namespace content {

class CONTENT_EXPORT WebServiceWorkerInstalledScriptsManagerImpl final
    : public blink::WebServiceWorkerInstalledScriptsManager {
 public:
  // Called on the main thread.
  static std::unique_ptr<blink::WebServiceWorkerInstalledScriptsManager> Create(
      mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ~WebServiceWorkerInstalledScriptsManagerImpl() override;

  // WebServiceWorkerInstalledScriptsManager implementation.
  bool IsScriptInstalled(const blink::WebURL& script_url) const override;
  std::unique_ptr<RawScriptData> GetRawScriptData(
      const blink::WebURL& script_url) override;

 private:
  WebServiceWorkerInstalledScriptsManagerImpl(
      std::vector<GURL>&& installed_urls,
      scoped_refptr<ThreadSafeScriptContainer> script_container,
      mojom::ServiceWorkerInstalledScriptsManagerHostPtr manager_host);

  const std::set<GURL> installed_urls_;
  scoped_refptr<ThreadSafeScriptContainer> script_container_;

  scoped_refptr<mojom::ThreadSafeServiceWorkerInstalledScriptsManagerHostPtr>
      manager_host_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_INSTALLED_SCRIPTS_MANAGER_IMPL_H_
