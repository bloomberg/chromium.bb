// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_test.h"

#include "base/debug/leak_annotations.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/autofill/content/renderer/test_password_generation_agent.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/renderer/render_view.h"
#include "extensions/features/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebScriptController.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/renderer/extensions/chrome_extensions_dispatcher_delegate.h"
#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"
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
using content::RenderFrame;
using testing::NiceMock;
using testing::Return;
using testing::_;

namespace {

// An autofill agent that treats all typing as user gesture.
class MockAutofillAgent : public AutofillAgent {
 public:
  MockAutofillAgent(RenderFrame* render_frame,
                    PasswordAutofillAgent* password_autofill_agent,
                    PasswordGenerationAgent* password_generation_agent,
                    service_manager::BinderRegistry* registry)
      : AutofillAgent(render_frame,
                      password_autofill_agent,
                      password_generation_agent,
                      registry) {
    ON_CALL(*this, IsUserGesture()).WillByDefault(Return(true));
  }

  ~MockAutofillAgent() override {}

  void WaitForAutofillDidAssociateFormControl() {
    DCHECK(run_loop_ == nullptr);
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    run_loop_.reset();
  }

  MOCK_CONST_METHOD0(IsUserGesture, bool());

 private:
  void DidAssociateFormControlsDynamically() override {
    AutofillAgent::DidAssociateFormControlsDynamically();
    if (run_loop_)
      run_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillAgent);
};

}  // namespace

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

  registry_ = base::MakeUnique<service_manager::BinderRegistry>();

  content::RenderViewTest::SetUp();

  RegisterMainFrameRemoteInterfaces();

  // RenderFrame doesn't expose its Agent objects, because it has no need to
  // store them directly (they're stored as RenderFrameObserver*).  So just
  // create another set.
  password_autofill_agent_ = new autofill::TestPasswordAutofillAgent(
      view_->GetMainRenderFrame(), registry_.get());
  password_generation_ = new autofill::TestPasswordGenerationAgent(
      view_->GetMainRenderFrame(), password_autofill_agent_, registry_.get());
  autofill_agent_ = new NiceMock<MockAutofillAgent>(
      view_->GetMainRenderFrame(), password_autofill_agent_,
      password_generation_, registry_.get());
}

void ChromeRenderViewTest::TearDown() {
  base::RunLoop().RunUntilIdle();

#if defined(LEAK_SANITIZER)
  // Do this before shutting down V8 in RenderViewTest::TearDown().
  // http://crbug.com/328552
  __lsan_do_leak_check();
#endif
  content::RenderViewTest::TearDown();
  registry_.reset();
}

content::ContentClient* ChromeRenderViewTest::CreateContentClient() {
  return new ChromeContentClient();
}

content::ContentBrowserClient*
ChromeRenderViewTest::CreateContentBrowserClient() {
  return new ChromeContentBrowserClient();
}

content::ContentRendererClient*
ChromeRenderViewTest::CreateContentRendererClient() {
  ChromeContentRendererClient* client = new ChromeContentRendererClient();
  InitChromeContentRendererClient(client);
  return client;
}

void ChromeRenderViewTest::RegisterMainFrameRemoteInterfaces() {}

void ChromeRenderViewTest::InitChromeContentRendererClient(
    ChromeContentRendererClient* client) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient* ext_client =
      ChromeExtensionsRendererClient::GetInstance();
  ext_client->SetExtensionDispatcherForTest(
      base::MakeUnique<extensions::Dispatcher>(
          std::make_unique<ChromeExtensionsDispatcherDelegate>()));
#endif
#if BUILDFLAG(ENABLE_SPELLCHECK)
  client->SetSpellcheck(new SpellCheck(nullptr));
#endif
}

void ChromeRenderViewTest::EnableUserGestureSimulationForAutofill() {
  EXPECT_CALL(*(static_cast<MockAutofillAgent*>(autofill_agent_)),
              IsUserGesture()).WillRepeatedly(Return(true));
}

void ChromeRenderViewTest::DisableUserGestureSimulationForAutofill() {
  EXPECT_CALL(*(static_cast<MockAutofillAgent*>(autofill_agent_)),
              IsUserGesture()).WillRepeatedly(Return(false));
}

void ChromeRenderViewTest::WaitForAutofillDidAssociateFormControl() {
  static_cast<MockAutofillAgent*>(autofill_agent_)
      ->WaitForAutofillDidAssociateFormControl();
}
