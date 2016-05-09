// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/test_runner_for_specific_view.h"

#include <stddef.h>
#include <limits>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/test_runner/app_banner_client.h"
#include "components/test_runner/layout_and_paint_async_then.h"
#include "components/test_runner/layout_dump.h"
#include "components/test_runner/mock_content_settings_client.h"
#include "components/test_runner/mock_credential_manager_client.h"
#include "components/test_runner/mock_screen_orientation_client.h"
#include "components/test_runner/mock_web_speech_recognizer.h"
#include "components/test_runner/mock_web_user_media_client.h"
#include "components/test_runner/pixel_dump.h"
#include "components/test_runner/spell_check_client.h"
#include "components/test_runner/test_common.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_preferences.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/web_task.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_proxy.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/platform/WebCanvas.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebPasswordCredential.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceMotionData.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceOrientationData.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "third_party/WebKit/public/web/WebArrayBuffer.h"
#include "third_party/WebKit/public/web/WebArrayBufferConverter.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPageImportanceSignals.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebSurroundingText.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/switches.h"

#if defined(__linux__) || defined(ANDROID)
#include "third_party/WebKit/public/web/linux/WebFontRendering.h"
#endif

using namespace blink;

namespace test_runner {

TestRunnerForSpecificView::TestRunnerForSpecificView(
    WebTestProxyBase* web_test_proxy_base)
    : web_test_proxy_base_(web_test_proxy_base), weak_factory_(this) {
  Reset();
}

TestRunnerForSpecificView::~TestRunnerForSpecificView() {}

void TestRunnerForSpecificView::Install(blink::WebLocalFrame* frame) {
  web_test_proxy_base_->test_interfaces()->GetTestRunner()->Install(
      frame, weak_factory_.GetWeakPtr());
}

void TestRunnerForSpecificView::Reset() {
  pointer_locked_ = false;
  pointer_lock_planned_result_ = PointerLockWillSucceed;

  if (web_view() && web_view()->mainFrame()) {
    RemoveWebPageOverlay();
    SetTabKeyCyclesThroughElements(true);

#if !defined(OS_MACOSX) && !defined(OS_WIN)
    // (Constants copied because we can't depend on the header that defined
    // them from this file.)
    web_view()->setSelectionColors(
        0xff1e90ff, 0xff000000, 0xffc8c8c8, 0xff323232);
#endif
    web_view()->setVisibilityState(WebPageVisibilityStateVisible, true);
    if (web_view()->mainFrame()->isWebLocalFrame()) {
      web_view()->mainFrame()->enableViewSourceMode(false);
      web_view()->setTextZoomFactor(1);
      web_view()->setZoomLevel(0);
    }
  }
}

bool TestRunnerForSpecificView::RequestPointerLock() {
  switch (pointer_lock_planned_result_) {
    case PointerLockWillSucceed:
      PostDelayedTask(
          0,
          base::Bind(&TestRunnerForSpecificView::DidAcquirePointerLockInternal,
                     weak_factory_.GetWeakPtr()));
      return true;
    case PointerLockWillRespondAsync:
      DCHECK(!pointer_locked_);
      return true;
    case PointerLockWillFailSync:
      DCHECK(!pointer_locked_);
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

void TestRunnerForSpecificView::RequestPointerUnlock() {
  PostDelayedTask(
      0, base::Bind(&TestRunnerForSpecificView::DidLosePointerLockInternal,
                    weak_factory_.GetWeakPtr()));
}

bool TestRunnerForSpecificView::isPointerLocked() {
  return pointer_locked_;
}

void TestRunnerForSpecificView::PostTask(const base::Closure& callback) {
  delegate()->PostTask(new WebCallbackTask(callback));
}

void TestRunnerForSpecificView::PostDelayedTask(long long delay,
                                                const base::Closure& callback) {
  delegate()->PostDelayedTask(new WebCallbackTask(callback), delay);
}

void TestRunnerForSpecificView::PostV8Callback(
    const v8::Local<v8::Function>& callback) {
  PostTask(base::Bind(&TestRunnerForSpecificView::InvokeV8Callback,
                      weak_factory_.GetWeakPtr(),
                      v8::UniquePersistent<v8::Function>(
                          blink::mainThreadIsolate(), callback)));
}

void TestRunnerForSpecificView::PostV8CallbackWithArgs(
    v8::UniquePersistent<v8::Function> callback,
    int argc,
    v8::Local<v8::Value> argv[]) {
  std::vector<v8::UniquePersistent<v8::Value>> args;
  for (int i = 0; i < argc; i++) {
    args.push_back(
        v8::UniquePersistent<v8::Value>(blink::mainThreadIsolate(), argv[i]));
  }

  PostTask(base::Bind(&TestRunnerForSpecificView::InvokeV8CallbackWithArgs,
                      weak_factory_.GetWeakPtr(), std::move(callback),
                      std::move(args)));
}

void TestRunnerForSpecificView::InvokeV8Callback(
    const v8::UniquePersistent<v8::Function>& callback) {
  std::vector<v8::UniquePersistent<v8::Value>> empty_args;
  InvokeV8CallbackWithArgs(callback, std::move(empty_args));
}

void TestRunnerForSpecificView::InvokeV8CallbackWithArgs(
    const v8::UniquePersistent<v8::Function>& callback,
    const std::vector<v8::UniquePersistent<v8::Value>>& args) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  WebFrame* frame = web_view()->mainFrame();
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> local_args;
  for (const auto& arg : args) {
    local_args.push_back(v8::Local<v8::Value>::New(isolate, arg));
  }

  frame->callFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>::New(isolate, callback), context->Global(),
      local_args.size(), local_args.data());
}

base::Closure TestRunnerForSpecificView::CreateClosureThatPostsV8Callback(
    const v8::Local<v8::Function>& callback) {
  return base::Bind(&TestRunnerForSpecificView::PostTask,
                    weak_factory_.GetWeakPtr(),
                    base::Bind(&TestRunnerForSpecificView::InvokeV8Callback,
                               weak_factory_.GetWeakPtr(),
                               v8::UniquePersistent<v8::Function>(
                                   blink::mainThreadIsolate(), callback)));
}

void TestRunnerForSpecificView::LayoutAndPaintAsync() {
  // TODO(lfg, lukasza): TestRunnerForSpecificView assumes that there's a single
  // WebWidget for the entire view, but with out-of-process iframes there may be
  // multiple WebWidgets, one for each local root. We should look into making
  // this structure more generic.
  test_runner::LayoutAndPaintAsyncThen(
      web_view()->mainFrame()->toWebLocalFrame()->frameWidget(),
      base::Closure());
}

void TestRunnerForSpecificView::LayoutAndPaintAsyncThen(
    v8::Local<v8::Function> callback) {
  test_runner::LayoutAndPaintAsyncThen(
      web_view()->mainFrame()->toWebLocalFrame()->frameWidget(),
      CreateClosureThatPostsV8Callback(callback));
}

void TestRunnerForSpecificView::CapturePixelsAsyncThen(
    v8::Local<v8::Function> callback) {
  v8::UniquePersistent<v8::Function> persistent_callback(
      blink::mainThreadIsolate(), callback);

  web_test_proxy_base_->test_interfaces()->GetTestRunner()->DumpPixelsAsync(
      web_view(), base::Bind(&TestRunnerForSpecificView::CapturePixelsCallback,
                             weak_factory_.GetWeakPtr(),
                             base::Passed(std::move(persistent_callback))));
}

void TestRunnerForSpecificView::CapturePixelsCallback(
    v8::UniquePersistent<v8::Function> callback,
    const SkBitmap& snapshot) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context =
      web_view()->mainFrame()->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> argv[3];
  SkAutoLockPixels snapshot_lock(snapshot);

  // Size can be 0 for cases where copyImageAt was called on position
  // that doesn't have an image.
  int width = snapshot.info().width();
  argv[0] = v8::Number::New(isolate, width);

  int height = snapshot.info().height();
  argv[1] = v8::Number::New(isolate, height);

  // Skia's internal byte order is platform-dependent. Always convert to RGBA
  // in order to provide a consistent ordering to the layout tests.
  const SkImageInfo bufferInfo =
      snapshot.info().makeColorType(kRGBA_8888_SkColorType);
  const size_t bufferRowBytes = bufferInfo.minRowBytes();
  blink::WebArrayBuffer buffer =
      blink::WebArrayBuffer::create(bufferInfo.getSafeSize(bufferRowBytes), 1);
  if (!snapshot.readPixels(bufferInfo,
                           buffer.data(),
                           bufferRowBytes,
                           0, 0)) {
    // We only expect readPixels to fail for null bitmaps.
    DCHECK(snapshot.isNull());
  }

  argv[2] = blink::WebArrayBufferConverter::toV8Value(
      &buffer, context->Global(), isolate);

  PostV8CallbackWithArgs(std::move(callback), arraysize(argv), argv);
}

void TestRunnerForSpecificView::CopyImageAtAndCapturePixelsAsyncThen(
    int x,
    int y,
    v8::Local<v8::Function> callback) {
  v8::UniquePersistent<v8::Function> persistent_callback(
      blink::mainThreadIsolate(), callback);

  CopyImageAtAndCapturePixels(
      web_view(), x, y,
      base::Bind(&TestRunnerForSpecificView::CapturePixelsCallback,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(persistent_callback))));
}

void TestRunnerForSpecificView::GetManifestThen(
    v8::Local<v8::Function> callback) {
  v8::UniquePersistent<v8::Function> persistent_callback(
      blink::mainThreadIsolate(), callback);

  delegate()->FetchManifest(
      web_view(), web_view()->mainFrame()->document().manifestURL(),
      base::Bind(&TestRunnerForSpecificView::GetManifestCallback,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(persistent_callback))));
}

void TestRunnerForSpecificView::GetManifestCallback(
    v8::UniquePersistent<v8::Function> callback,
    const blink::WebURLResponse& response,
    const std::string& data) {
  PostV8CallbackWithArgs(std::move(callback), 0, nullptr);
}

void TestRunnerForSpecificView::GetBluetoothManualChooserEvents(
    v8::Local<v8::Function> callback) {
  return delegate()->GetBluetoothManualChooserEvents(base::Bind(
      &TestRunnerForSpecificView::GetBluetoothManualChooserEventsCallback,
      weak_factory_.GetWeakPtr(),
      base::Passed(v8::UniquePersistent<v8::Function>(
          blink::mainThreadIsolate(), callback))));
}

void TestRunnerForSpecificView::GetBluetoothManualChooserEventsCallback(
    v8::UniquePersistent<v8::Function> callback,
    const std::vector<std::string>& events) {
  // Build the V8 context.
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      web_view()->mainFrame()->mainWorldScriptContext();
  if (context.IsEmpty())
    return;
  v8::Context::Scope context_scope(context);

  // Convert the argument.
  v8::Local<v8::Value> arg;
  if (!gin::TryConvertToV8(isolate, events, &arg))
    return;

  // Call the callback.
  PostV8CallbackWithArgs(std::move(callback), 1, &arg);
}

void TestRunnerForSpecificView::SetBluetoothFakeAdapter(
    const std::string& adapter_name,
    v8::Local<v8::Function> callback) {
  delegate()->SetBluetoothFakeAdapter(
      adapter_name, CreateClosureThatPostsV8Callback(callback));
}

void TestRunnerForSpecificView::SetBluetoothManualChooser(bool enable) {
  delegate()->SetBluetoothManualChooser(enable);
}

void TestRunnerForSpecificView::SendBluetoothManualChooserEvent(
    const std::string& event,
    const std::string& argument) {
  delegate()->SendBluetoothManualChooserEvent(event, argument);
}

void TestRunnerForSpecificView::SetBackingScaleFactor(
    double value,
    v8::Local<v8::Function> callback) {
  delegate()->SetDeviceScaleFactor(value);
  PostV8Callback(callback);
}

void TestRunnerForSpecificView::EnableUseZoomForDSF(
    v8::Local<v8::Function> callback) {
  delegate()->EnableUseZoomForDSF();
  PostV8Callback(callback);
}

void TestRunnerForSpecificView::SetColorProfile(
    const std::string& name,
    v8::Local<v8::Function> callback) {
  delegate()->SetDeviceColorProfile(name);
  PostV8Callback(callback);
}

void TestRunnerForSpecificView::DispatchBeforeInstallPromptEvent(
    int request_id,
    const std::vector<std::string>& event_platforms,
    v8::Local<v8::Function> callback) {
  delegate()->DispatchBeforeInstallPromptEvent(
      request_id, event_platforms,
      base::Bind(
          &TestRunnerForSpecificView::DispatchBeforeInstallPromptCallback,
          weak_factory_.GetWeakPtr(),
          base::Passed(v8::UniquePersistent<v8::Function>(
              blink::mainThreadIsolate(), callback))));
}

void TestRunnerForSpecificView::DispatchBeforeInstallPromptCallback(
    v8::UniquePersistent<v8::Function> callback,
    bool canceled) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context =
      web_view()->mainFrame()->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> arg;
  arg = v8::Boolean::New(isolate, canceled);

  PostV8CallbackWithArgs(std::move(callback), 1, &arg);
}

void TestRunnerForSpecificView::RunIdleTasks(v8::Local<v8::Function> callback) {
  delegate()->RunIdleTasks(CreateClosureThatPostsV8Callback(callback));
}

void TestRunnerForSpecificView::SetTabKeyCyclesThroughElements(
    bool tab_key_cycles_through_elements) {
  web_view()->setTabKeyCyclesThroughElements(tab_key_cycles_through_elements);
}

void TestRunnerForSpecificView::ExecCommand(gin::Arguments* args) {
  std::string command;
  args->GetNext(&command);

  std::string value;
  if (args->Length() >= 3) {
    // Ignore the second parameter (which is userInterface)
    // since this command emulates a manual action.
    args->Skip();
    args->GetNext(&value);
  }

  // Note: webkit's version does not return the boolean, so neither do we.
  web_view()->focusedFrame()->executeCommand(WebString::fromUTF8(command),
                                             WebString::fromUTF8(value));
}

bool TestRunnerForSpecificView::IsCommandEnabled(const std::string& command) {
  return web_view()->focusedFrame()->isCommandEnabled(
      WebString::fromUTF8(command));
}

bool TestRunnerForSpecificView::HasCustomPageSizeStyle(int page_index) {
  WebFrame* frame = web_view()->mainFrame();
  if (!frame)
    return false;
  return frame->hasCustomPageSizeStyle(page_index);
}

void TestRunnerForSpecificView::ForceRedSelectionColors() {
  web_view()->setSelectionColors(
      0xffee0000, 0xff00ee00, 0xff000000, 0xffc0c0c0);
}

void TestRunnerForSpecificView::SetPageVisibility(
    const std::string& new_visibility) {
  if (new_visibility == "visible")
    web_view()->setVisibilityState(WebPageVisibilityStateVisible, false);
  else if (new_visibility == "hidden")
    web_view()->setVisibilityState(WebPageVisibilityStateHidden, false);
  else if (new_visibility == "prerender")
    web_view()->setVisibilityState(WebPageVisibilityStatePrerender, false);
}

void TestRunnerForSpecificView::SetTextDirection(
    const std::string& direction_name) {
  // Map a direction name to a WebTextDirection value.
  WebTextDirection direction;
  if (direction_name == "auto")
    direction = WebTextDirectionDefault;
  else if (direction_name == "rtl")
    direction = WebTextDirectionRightToLeft;
  else if (direction_name == "ltr")
    direction = WebTextDirectionLeftToRight;
  else
    return;

  web_view()->setTextDirection(direction);
}

void TestRunnerForSpecificView::DumpPageImportanceSignals() {
  blink::WebPageImportanceSignals* signals =
      web_view()->pageImportanceSignals();
  if (!signals)
    return;

  std::string message = base::StringPrintf(
      "WebPageImportanceSignals:\n"
      "  hadFormInteraction: %s\n"
      "  issuedNonGetFetchFromScript: %s\n",
      signals->hadFormInteraction() ? "true" : "false",
      signals->issuedNonGetFetchFromScript() ? "true" : "false");
  if (delegate())
    delegate()->PrintMessage(message);
}

void TestRunnerForSpecificView::AddWebPageOverlay() {
  web_view()->setPageOverlayColor(SK_ColorCYAN);
}

void TestRunnerForSpecificView::RemoveWebPageOverlay() {
  web_view()->setPageOverlayColor(SK_ColorTRANSPARENT);
}

void TestRunnerForSpecificView::ForceNextWebGLContextCreationToFail() {
  web_view()->forceNextWebGLContextCreationToFail();
}

void TestRunnerForSpecificView::ForceNextDrawingBufferCreationToFail() {
  web_view()->forceNextDrawingBufferCreationToFail();
}

void TestRunnerForSpecificView::SetWindowIsKey(bool value) {
  web_test_proxy_base_->test_interfaces()->GetTestRunner()->SetFocus(web_view(),
                                                                     value);
}

void TestRunnerForSpecificView::DidAcquirePointerLock() {
  DidAcquirePointerLockInternal();
}

void TestRunnerForSpecificView::DidNotAcquirePointerLock() {
  DidNotAcquirePointerLockInternal();
}

void TestRunnerForSpecificView::DidLosePointerLock() {
  DidLosePointerLockInternal();
}

void TestRunnerForSpecificView::SetPointerLockWillFailSynchronously() {
  pointer_lock_planned_result_ = PointerLockWillFailSync;
}

void TestRunnerForSpecificView::SetPointerLockWillRespondAsynchronously() {
  pointer_lock_planned_result_ = PointerLockWillRespondAsync;
}

void TestRunnerForSpecificView::DidAcquirePointerLockInternal() {
  pointer_locked_ = true;
  web_view()->didAcquirePointerLock();

  // Reset planned result to default.
  pointer_lock_planned_result_ = PointerLockWillSucceed;
}

void TestRunnerForSpecificView::DidNotAcquirePointerLockInternal() {
  DCHECK(!pointer_locked_);
  pointer_locked_ = false;
  web_view()->didNotAcquirePointerLock();

  // Reset planned result to default.
  pointer_lock_planned_result_ = PointerLockWillSucceed;
}

void TestRunnerForSpecificView::DidLosePointerLockInternal() {
  bool was_locked = pointer_locked_;
  pointer_locked_ = false;
  if (was_locked)
    web_view()->didLosePointerLock();
}

bool TestRunnerForSpecificView::CallShouldCloseOnWebView() {
  return web_view()->mainFrame()->dispatchBeforeUnloadEvent();
}

void TestRunnerForSpecificView::SetDomainRelaxationForbiddenForURLScheme(
    bool forbidden,
    const std::string& scheme) {
  web_view()->setDomainRelaxationForbidden(forbidden,
                                           WebString::fromUTF8(scheme));
}

v8::Local<v8::Value>
TestRunnerForSpecificView::EvaluateScriptInIsolatedWorldAndReturnValue(
    int world_id,
    const std::string& script) {
  WebVector<v8::Local<v8::Value>> values;
  WebScriptSource source(WebString::fromUTF8(script));
  // This relies on the iframe focusing itself when it loads. This is a bit
  // sketchy, but it seems to be what other tests do.
  web_view()->focusedFrame()->executeScriptInIsolatedWorld(world_id, &source, 1,
                                                           1, &values);
  // Since only one script was added, only one result is expected
  if (values.size() == 1 && !values[0].IsEmpty())
    return values[0];
  return v8::Local<v8::Value>();
}

void TestRunnerForSpecificView::EvaluateScriptInIsolatedWorld(
    int world_id,
    const std::string& script) {
  WebScriptSource source(WebString::fromUTF8(script));
  web_view()->focusedFrame()->executeScriptInIsolatedWorld(world_id, &source, 1,
                                                           1);
}

void TestRunnerForSpecificView::SetIsolatedWorldSecurityOrigin(
    int world_id,
    v8::Local<v8::Value> origin) {
  if (!(origin->IsString() || !origin->IsNull()))
    return;

  WebSecurityOrigin web_origin;
  if (origin->IsString()) {
    web_origin = WebSecurityOrigin::createFromString(
        V8StringToWebString(origin.As<v8::String>()));
  }
  web_view()->focusedFrame()->setIsolatedWorldSecurityOrigin(world_id,
                                                             web_origin);
}

void TestRunnerForSpecificView::SetIsolatedWorldContentSecurityPolicy(
    int world_id,
    const std::string& policy) {
  web_view()->focusedFrame()->setIsolatedWorldContentSecurityPolicy(
      world_id, WebString::fromUTF8(policy));
}

void TestRunner::InsertStyleSheet(const std::string& source_code) {
  WebLocalFrame::frameForCurrentContext()->document().insertStyleSheet(
      WebString::fromUTF8(source_code));
}

bool TestRunnerForSpecificView::FindString(
    const std::string& search_text,
    const std::vector<std::string>& options_array) {
  WebFindOptions find_options;
  bool wrap_around = false;
  find_options.matchCase = true;
  find_options.findNext = true;

  for (const std::string& option : options_array) {
    if (option == "CaseInsensitive")
      find_options.matchCase = false;
    else if (option == "Backwards")
      find_options.forward = false;
    else if (option == "StartInSelection")
      find_options.findNext = false;
    else if (option == "AtWordStarts")
      find_options.wordStart = true;
    else if (option == "TreatMedialCapitalAsWordStart")
      find_options.medialCapitalAsWordStart = true;
    else if (option == "WrapAround")
      wrap_around = true;
  }

  WebLocalFrame* frame = web_view()->mainFrame()->toWebLocalFrame();
  const bool find_result = frame->find(0, WebString::fromUTF8(search_text),
                                       find_options, wrap_around, 0);
  frame->stopFinding(false);
  return find_result;
}

std::string TestRunnerForSpecificView::SelectionAsMarkup() {
  return web_view()->mainFrame()->selectionAsMarkup().utf8();
}

void TestRunnerForSpecificView::SetViewSourceForFrame(const std::string& name,
                                                      bool enabled) {
  WebFrame* target_frame =
      web_view()->findFrameByName(WebString::fromUTF8(name));
  if (target_frame)
    target_frame->enableViewSourceMode(enabled);
}

blink::WebView* TestRunnerForSpecificView::web_view() {
  return web_test_proxy_base_->web_view();
}

WebTestDelegate* TestRunnerForSpecificView::delegate() {
  return web_test_proxy_base_->delegate();
}

}  // namespace test_runner
