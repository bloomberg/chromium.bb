// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/renderer/extensions/dispatcher.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_custom_bindings.h"
#include "chrome/renderer/extensions/miscellaneous_bindings.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_LINUX) && !defined(USE_AURA)
#include "ui/base/gtk/event_synthesis_gtk.h"
#endif

using extensions::ExtensionCustomBindings;
using extensions::MiscellaneousBindings;
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
  content::GetContentClient()->set_renderer_for_testing(
      &chrome_content_renderer_client_);
  extension_dispatcher_ = new extensions::Dispatcher();
  chrome_content_renderer_client_.SetExtensionDispatcher(extension_dispatcher_);
  chrome_content_renderer_client_.SetSpellcheck(new SpellCheck());

  chrome_render_thread_ = new ChromeMockRenderThread();
  render_thread_.reset(chrome_render_thread_);
  content::RenderViewTest::SetUp();

  // RenderView doesn't expose it's PasswordAutofillManager or
  // AutofillAgent objects, because it has no need to store them directly
  // (they're stored as RenderViewObserver*).  So just create another set.
  password_autofill_ = new PasswordAutofillManager(view_);
  autofill_agent_ = new AutofillAgent(view_, password_autofill_);
}

void ChromeRenderViewTest::TearDown() {
  content::RenderViewTest::TearDown();

  extension_dispatcher_->OnRenderProcessShutdown();
  extension_dispatcher_ = NULL;
}
