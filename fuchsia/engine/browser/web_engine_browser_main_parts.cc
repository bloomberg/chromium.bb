// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_browser_main_parts.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/main_function_params.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "fuchsia/engine/browser/web_engine_screen.h"
#include "fuchsia/engine/common.h"
#include "ui/aura/screen_ozone.h"
#include "ui/ozone/public/ozone_platform.h"

WebEngineBrowserMainParts::WebEngineBrowserMainParts(
    const content::MainFunctionParams& parameters,
    fidl::InterfaceRequest<fuchsia::web::Context> request)
    : parameters_(parameters), request_(std::move(request)) {}

WebEngineBrowserMainParts::~WebEngineBrowserMainParts() {
  display::Screen::SetScreenInstance(nullptr);
}

void WebEngineBrowserMainParts::PreMainMessageLoopRun() {
  DCHECK(!screen_);

  auto platform_screen = ui::OzonePlatform::GetInstance()->CreateScreen();
  if (platform_screen) {
    screen_ = std::make_unique<aura::ScreenOzone>(std::move(platform_screen));
  } else {
    // Use dummy display::Screen for Ozone platforms that don't provide
    // PlatformScreen.
    screen_ = std::make_unique<WebEngineScreen>();
  }

  display::Screen::SetScreenInstance(screen_.get());

  DCHECK(!browser_context_);
  browser_context_ = std::make_unique<WebEngineBrowserContext>(
      base::CommandLine::ForCurrentProcess()->HasSwitch(kIncognitoSwitch));

  DCHECK(request_);
  context_service_ = std::make_unique<ContextImpl>(browser_context_.get());
  context_binding_ = std::make_unique<fidl::Binding<fuchsia::web::Context>>(
      context_service_.get(), std::move(request_));

  // Quit the browser main loop when the Context connection is dropped.
  context_binding_->set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " Context disconnected.";
    context_service_.reset();
    std::move(quit_closure_).Run();
  });

  // Disable RenderFrameHost's Javascript injection restrictions so that the
  // Context and Frames can implement their own JS injection policy at a higher
  // level.
  content::RenderFrameHost::AllowInjectingJavaScript();

  if (parameters_.ui_task) {
    // Since the main loop won't run, there is nothing to quit in the
    // |context_binding_| error handler.
    quit_closure_ = base::DoNothing::Once();

    parameters_.ui_task->Run();
    delete parameters_.ui_task;
    run_message_loop_ = false;
  }
}

void WebEngineBrowserMainParts::PreDefaultMainMessageLoopRun(
    base::OnceClosure quit_closure) {
  quit_closure_ = std::move(quit_closure);
}

bool WebEngineBrowserMainParts::MainMessageLoopRun(int* result_code) {
  return !run_message_loop_;
}

void WebEngineBrowserMainParts::PostMainMessageLoopRun() {
  // The service and its binding should have already been released by the error
  // handler.
  DCHECK(!context_service_);
  DCHECK(!context_binding_->is_bound());

  // These resources must be freed while a MessageLoop is still available, so
  // that they may post cleanup tasks during teardown.
  // NOTE: Please destroy objects in the reverse order of their creation.
  browser_context_.reset();
  screen_.reset();
}
