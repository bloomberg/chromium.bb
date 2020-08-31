// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/common/web_content_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/cpp/component_context.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/startup_context.h"
#include "base/logging.h"
#include "fuchsia/runners/buildflags.h"
#include "fuchsia/runners/common/web_component.h"
#include "url/gurl.h"

namespace {

fuchsia::web::ContextPtr CreateWebContext(
    fuchsia::web::CreateContextParams context_params) {
  auto context_provider = base::fuchsia::ComponentContextForCurrentProcess()
                              ->svc()
                              ->Connect<fuchsia::web::ContextProvider>();
  fuchsia::web::ContextPtr web_context;
  context_provider->Create(std::move(context_params), web_context.NewRequest());

  return web_context;
}

}  // namespace

WebContentRunner::WebContentRunner(
    GetContextParamsCallback get_context_params_callback)
    : get_context_params_callback_(std::move(get_context_params_callback)) {}

WebContentRunner::WebContentRunner(
    fuchsia::web::CreateContextParams context_params)
    : context_(CreateWebContext(std::move(context_params))) {
  context_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "Connection to one-shot Context lost.";
  });
}

WebContentRunner::~WebContentRunner() = default;

fuchsia::web::FramePtr WebContentRunner::CreateFrame(
    fuchsia::web::CreateFrameParams params) {
  if (!context_) {
    DCHECK(get_context_params_callback_);
    context_ = CreateWebContext(get_context_params_callback_.Run());
    context_.set_error_handler([this](zx_status_t status) {
      ZX_LOG(ERROR, status) << "Connection to Context lost.";
      if (on_context_lost_callback_) {
        std::move(on_context_lost_callback_).Run();
      }
    });
  }

  fuchsia::web::FramePtr frame;
  context_->CreateFrameWithParams(std::move(params), frame.NewRequest());
  return frame;
}

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

  std::unique_ptr<WebComponent> component = std::make_unique<WebComponent>(
      this,
      std::make_unique<base::fuchsia::StartupContext>(std::move(startup_info)),
      std::move(controller_request));
#if BUILDFLAG(WEB_RUNNER_REMOTE_DEBUGGING_PORT) != 0
  component->EnableRemoteDebugging();
#endif
  component->StartComponent();
  component->LoadUrl(url, std::vector<fuchsia::net::http::Header>());
  RegisterComponent(std::move(component));
}

WebComponent* WebContentRunner::GetAnyComponent() {
  if (components_.empty())
    return nullptr;

  return components_.begin()->get();
}

void WebContentRunner::DestroyComponent(WebComponent* component) {
  components_.erase(components_.find(component));
  if (components_.empty() && on_empty_callback_)
    std::move(on_empty_callback_).Run();
}

void WebContentRunner::RegisterComponent(
    std::unique_ptr<WebComponent> component) {
  components_.insert(std::move(component));
}

void WebContentRunner::SetOnEmptyCallback(base::OnceClosure on_empty) {
  on_empty_callback_ = std::move(on_empty);
}

void WebContentRunner::SetOnContextLostCallbackForTest(
    base::OnceClosure callback) {
  on_context_lost_callback_ = std::move(callback);
}
