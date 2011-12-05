// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_test.h"

#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/print_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/autofill/password_autofill_manager.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/miscellaneous_bindings.h"
#include "chrome/renderer/extensions/schema_generated_bindings.h"
#include "content/common/dom_storage_common.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/renderer_preferences.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_main_platform_delegate.h"
#include "content/test/mock_render_process.h"
#include "content/test/mock_render_thread.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_LINUX) && !defined(USE_AURA)
#include "ui/base/gtk/event_synthesis_gtk.h"
#endif

using extensions::MiscellaneousBindings;
using extensions::SchemaGeneratedBindings;
using WebKit::WebFrame;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebScriptController;
using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebURLRequest;
using autofill::AutofillAgent;
using autofill::PasswordAutofillManager;

ChromeRenderViewTest::ChromeRenderViewTest() : extension_dispatcher_(NULL) {
}

ChromeRenderViewTest::~ChromeRenderViewTest() {
}

void ChromeRenderViewTest::SetUp() {
  content::GetContentClient()->set_renderer(&chrome_content_renderer_client_);
  extension_dispatcher_ = new ExtensionDispatcher();
  chrome_content_renderer_client_.SetExtensionDispatcher(extension_dispatcher_);

  chrome_render_thread_ = new ChromeMockRenderThread();
  render_thread_.reset(chrome_render_thread_);
  content::RenderViewTest::SetUp();

  WebScriptController::registerExtension(new ChromeV8Extension(
      "extensions/json_schema.js", IDR_JSON_SCHEMA_JS, NULL));
  WebScriptController::registerExtension(EventBindings::Get(
      extension_dispatcher_));
  WebScriptController::registerExtension(MiscellaneousBindings::Get(
      extension_dispatcher_));
  WebScriptController::registerExtension(SchemaGeneratedBindings::Get(
      extension_dispatcher_));
  WebScriptController::registerExtension(new ChromeV8Extension(
      "extensions/apitest.js", IDR_EXTENSION_APITEST_JS, NULL));

  // RenderView doesn't expose it's PasswordAutofillManager or
  // AutofillAgent objects, because it has no need to store them directly
  // (they're stored as RenderViewObserver*).  So just create another set.
  password_autofill_ = new PasswordAutofillManager(view_);
  autofill_agent_ = new AutofillAgent(view_, password_autofill_);
}

void ChromeRenderViewTest::TearDown() {
  content::RenderViewTest::TearDown();

  render_thread_->SendCloseMessage();

  extension_dispatcher_->OnRenderProcessShutdown();
  extension_dispatcher_ = NULL;
}
