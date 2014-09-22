// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_test.h"

#include "base/debug/leak_annotations.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/autofill/content/renderer/test_password_generation_agent.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebScriptController.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/renderer/extensions/chrome_extensions_dispatcher_delegate.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/event_bindings.h"
#endif

using autofill::AutofillAgent;
using autofill::PasswordAutofillAgent;
using autofill::PasswordGenerationAgent;
using blink::WebFrame;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebScriptController;
using blink::WebScriptSource;
using blink::WebString;
using blink::WebURLRequest;

ChromeRenderViewTest::ChromeRenderViewTest()
    : password_autofill_agent_(NULL),
      password_generation_(NULL),
      autofill_agent_(NULL),
      chrome_render_thread_(NULL) {
}

ChromeRenderViewTest::~ChromeRenderViewTest() {
}

void ChromeRenderViewTest::SetUp() {
  ChromeUnitTestSuite::InitializeProviders();
  ChromeUnitTestSuite::InitializeResourceBundle();

  chrome_render_thread_ = new ChromeMockRenderThread();
  render_thread_.reset(chrome_render_thread_);

  content::RenderViewTest::SetUp();

  // RenderView doesn't expose its Agent objects, because it has no need to
  // store them directly (they're stored as RenderViewObserver*).  So just
  // create another set.
  password_autofill_agent_ = new autofill::TestPasswordAutofillAgent(view_);
  password_generation_ = new autofill::TestPasswordGenerationAgent(view_);
  autofill_agent_ =
      new AutofillAgent(view_, password_autofill_agent_, password_generation_);
}

void ChromeRenderViewTest::TearDown() {
#if defined(ENABLE_EXTENSIONS)
  ChromeContentRendererClient* client =
      static_cast<ChromeContentRendererClient*>(content_renderer_client_.get());
  client->GetExtensionDispatcherForTest()->OnRenderProcessShutdown();
#endif

#if defined(LEAK_SANITIZER)
  // Do this before shutting down V8 in RenderViewTest::TearDown().
  // http://crbug.com/328552
  __lsan_do_leak_check();
#endif
  content::RenderViewTest::TearDown();
}

content::ContentClient* ChromeRenderViewTest::CreateContentClient() {
  return new ChromeContentClient();
}

content::ContentBrowserClient*
    ChromeRenderViewTest::CreateContentBrowserClient() {
  return new chrome::ChromeContentBrowserClient();
}

content::ContentRendererClient*
    ChromeRenderViewTest::CreateContentRendererClient() {
  ChromeContentRendererClient* client = new ChromeContentRendererClient();
#if defined(ENABLE_EXTENSIONS)
  extension_dispatcher_delegate_.reset(
      new ChromeExtensionsDispatcherDelegate());
  client->SetExtensionDispatcherForTest(
      new extensions::Dispatcher(extension_dispatcher_delegate_.get()));
#endif
#if defined(ENABLE_SPELLCHECK)
  client->SetSpellcheck(new SpellCheck());
#endif
  return client;
}
