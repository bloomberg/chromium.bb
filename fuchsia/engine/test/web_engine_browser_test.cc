// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/test/web_engine_browser_test.h"

#include <fuchsia/web/cpp/fidl.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "fuchsia/engine/browser/web_engine_browser_main_parts.h"
#include "fuchsia/engine/browser/web_engine_content_browser_client.h"
#include "fuchsia/engine/legacy_context_bridge.h"
#include "fuchsia/engine/web_engine_main_delegate.h"
#include "net/test/embedded_test_server/default_handlers.h"

namespace cr_fuchsia {

namespace {
zx_handle_t g_context_channel = ZX_HANDLE_INVALID;
}  // namespace

WebEngineBrowserTest::WebEngineBrowserTest() = default;

WebEngineBrowserTest::~WebEngineBrowserTest() = default;

void WebEngineBrowserTest::PreRunTestOnMainThread() {
  fuchsia::web::ContextPtr fuchsia_context;
  zx_status_t result = fuchsia_context.Bind(zx::channel(g_context_channel));
  ZX_DCHECK(result == ZX_OK, result) << "Context::Bind";
  g_context_channel = ZX_HANDLE_INVALID;

  new LegacyContextBridge(context_.NewRequest(), std::move(fuchsia_context));

  net::test_server::RegisterDefaultHandlers(embedded_test_server());
  if (!test_server_root_.empty()) {
    embedded_test_server()->ServeFilesFromSourceDirectory(test_server_root_);
  }
}

void WebEngineBrowserTest::PostRunTestOnMainThread() {
  // Unbind the Context while the message loops are still alive.
  context_.Unbind();
}

void WebEngineBrowserTest::TearDownOnMainThread() {
  navigation_observer_bindings_.CloseAll();
}

chromium::web::FramePtr WebEngineBrowserTest::CreateFrame(
    chromium::web::NavigationEventObserver* observer) {
  chromium::web::FramePtr frame;
  context_->CreateFrame(frame.NewRequest());

  if (observer) {
    fidl::InterfaceRequest<chromium::web::NavigationEventObserver>
        observer_request;
    frame->SetNavigationEventObserver(
        navigation_observer_bindings_.AddBinding(observer));
  }

  // Pump the messages so that the caller can use the Frame instance
  // immediately after this function returns.
  base::RunLoop().RunUntilIdle();

  return frame;
}

// static
void WebEngineBrowserTest::SetContextClientChannel(zx::channel channel) {
  DCHECK(channel);
  g_context_channel = channel.release();
}

ContextImpl* WebEngineBrowserTest::context_impl() const {
  return WebEngineMainDelegate::GetInstanceForTest()
      ->browser_client()
      ->main_parts_for_test()
      ->context_for_test();
}

}  // namespace cr_fuchsia
