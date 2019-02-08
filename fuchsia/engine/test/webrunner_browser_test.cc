// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/test/webrunner_browser_test.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "fuchsia/engine/browser/webrunner_browser_context.h"
#include "fuchsia/engine/browser/webrunner_browser_main_parts.h"
#include "fuchsia/engine/browser/webrunner_content_browser_client.h"
#include "fuchsia/engine/webrunner_main_delegate.h"
#include "net/test/embedded_test_server/default_handlers.h"

namespace cr_fuchsia {
namespace test {

namespace {
zx_handle_t g_context_channel = ZX_HANDLE_INVALID;
}  // namespace

WebRunnerBrowserTest::WebRunnerBrowserTest() = default;

WebRunnerBrowserTest::~WebRunnerBrowserTest() = default;

void WebRunnerBrowserTest::PreRunTestOnMainThread() {
  zx_status_t result = context_.Bind(zx::channel(g_context_channel));
  ZX_DCHECK(result == ZX_OK, result) << "Context::Bind";
  g_context_channel = ZX_HANDLE_INVALID;

  net::test_server::RegisterDefaultHandlers(embedded_test_server());
  if (!test_server_root_.empty()) {
    embedded_test_server()->ServeFilesFromSourceDirectory(test_server_root_);
  }
}

void WebRunnerBrowserTest::PostRunTestOnMainThread() {
  // Unbind the Context while the message loops are still alive.
  context_.Unbind();
}

void WebRunnerBrowserTest::TearDownOnMainThread() {
  navigation_observer_bindings_.CloseAll();
}

chromium::web::FramePtr WebRunnerBrowserTest::CreateFrame(
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
void WebRunnerBrowserTest::SetContextClientChannel(zx::channel channel) {
  DCHECK(channel);
  g_context_channel = channel.release();
}

ContextImpl* WebRunnerBrowserTest::context_impl() const {
  return WebRunnerMainDelegate::GetInstanceForTest()
      ->browser_client()
      ->main_parts_for_test()
      ->context();
}

}  // namespace test
}  // namespace cr_fuchsia
