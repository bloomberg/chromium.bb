// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/common/web_content_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <utility>

#include "base/files/file.h"
#include "base/fuchsia/component_context.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/logging.h"
#include "fuchsia/runners/common/web_component.h"
#include "url/gurl.h"

// static
chromium::web::ContextPtr WebContentRunner::CreateDefaultWebContext() {
  auto web_context_provider =
      base::fuchsia::ComponentContext::GetDefault()
          ->ConnectToService<chromium::web::ContextProvider>();

  chromium::web::CreateContextParams create_params;

  // Clone /svc to the context.
  create_params.service_directory =
      zx::channel(base::fuchsia::GetHandleFromFile(
          base::File(base::FilePath("/svc"),
                     base::File::FLAG_OPEN | base::File::FLAG_READ)));

  chromium::web::ContextPtr web_context;
  web_context_provider->Create(std::move(create_params),
                               web_context.NewRequest());
  web_context.set_error_handler([](zx_status_t status) {
    // If the browser instance died, then exit everything and do not attempt
    // to recover. appmgr will relaunch the runner when it is needed again.
    ZX_LOG(ERROR, status) << "Connection to Context lost.";
    exit(1);
  });
  return web_context;
}

WebContentRunner::WebContentRunner(
    base::fuchsia::ServiceDirectory* service_directory,
    chromium::web::ContextPtr context,
    base::OnceClosure on_idle_closure)
    : context_(std::move(context)),
      service_binding_(service_directory, this),
      on_idle_closure_(std::move(on_idle_closure)) {
  DCHECK(context_);
  DCHECK(on_idle_closure_);

  // Signal that we're idle if the service manager connection is dropped.
  service_binding_.SetOnLastClientCallback(base::BindOnce(
      &WebContentRunner::RunOnIdleClosureIfValid, base::Unretained(this)));
}

WebContentRunner::~WebContentRunner() = default;

void WebContentRunner::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  GURL url(package.resolved_url);
  if (!url.is_valid()) {
    LOG(ERROR) << "Rejected invalid URL: " << url;
    return;
  }

  RegisterComponent(WebComponent::ForUrlRequest(this, std::move(url),
                                                std::move(startup_info),
                                                std::move(controller_request)));
}

void WebContentRunner::GetWebComponentForTest(
    base::OnceCallback<void(WebComponent*)> callback) {
  if (!components_.empty()) {
    std::move(callback).Run(components_.begin()->get());
    return;
  }
  web_component_test_callback_ = std::move(callback);
}

void WebContentRunner::DestroyComponent(WebComponent* component) {
  components_.erase(components_.find(component));

  if (components_.empty())
    RunOnIdleClosureIfValid();
}

void WebContentRunner::RegisterComponent(
    std::unique_ptr<WebComponent> component) {
  if (web_component_test_callback_) {
    std::move(web_component_test_callback_).Run(component.get());
  }
  if (component) {
    components_.insert(std::move(component));
  }
}

void WebContentRunner::RunOnIdleClosureIfValid() {
  if (on_idle_closure_)
    std::move(on_idle_closure_).Run();
}
