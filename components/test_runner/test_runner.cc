// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/test_runner.h"

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
#include "components/test_runner/test_runner_for_specific_view.h"
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
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

#if defined(__linux__) || defined(ANDROID)
#include "third_party/WebKit/public/web/linux/WebFontRendering.h"
#endif

using namespace blink;

namespace test_runner {

namespace {

double GetDefaultDeviceScaleFactor() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceDeviceScaleFactor)) {
    double scale;
    std::string value =
        command_line->GetSwitchValueASCII(switches::kForceDeviceScaleFactor);
    if (base::StringToDouble(value, &scale))
      return scale;
  }
  return 1.f;
}

}  // namespace

class TestRunnerBindings : public gin::Wrappable<TestRunnerBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(base::WeakPtr<TestRunner> test_runner,
                      base::WeakPtr<TestRunnerForSpecificView> view_test_runner,
                      WebLocalFrame* frame);

 private:
  explicit TestRunnerBindings(
      base::WeakPtr<TestRunner> test_runner,
      base::WeakPtr<TestRunnerForSpecificView> view_test_runner);
  ~TestRunnerBindings() override;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  void AddMockSpeechRecognitionResult(const std::string& transcript,
                                      double confidence);
  void AddOriginAccessWhitelistEntry(const std::string& source_origin,
                                     const std::string& destination_protocol,
                                     const std::string& destination_host,
                                     bool allow_destination_subdomains);
  void AddWebPageOverlay();
  void CapturePixelsAsyncThen(v8::Local<v8::Function> callback);
  void ClearAllDatabases();
  void ClearGeofencingMockProvider();
  void ClearPrinting();
  void CloseWebInspector();
  void CopyImageAtAndCapturePixelsAsyncThen(int x,
                                            int y,
                                            v8::Local<v8::Function> callback);
  void DidAcquirePointerLock();
  void DidLosePointerLock();
  void DidNotAcquirePointerLock();
  void DisableMockScreenOrientation();
  void DispatchBeforeInstallPromptEvent(
      int request_id,
      const std::vector<std::string>& event_platforms,
      v8::Local<v8::Function> callback);
  void DumpAsMarkup();
  void DumpAsText();
  void DumpAsTextWithPixelResults();
  void DumpBackForwardList();
  void DumpChildFrameScrollPositions();
  void DumpChildFramesAsMarkup();
  void DumpChildFramesAsText();
  void DumpCreateView();
  void DumpDragImage();
  void DumpEditingCallbacks();
  void DumpFrameLoadCallbacks();
  void DumpIconChanges();
  void DumpNavigationPolicy();
  void DumpPageImportanceSignals();
  void DumpPermissionClientCallbacks();
  void DumpPingLoaderCallbacks();
  void DumpResourceLoadCallbacks();
  void DumpResourceRequestPriorities();
  void DumpResourceResponseMIMETypes();
  void DumpSelectionRect();
  void DumpSpellCheckCallbacks();
  void DumpTitleChanges();
  void DumpUserGestureInFrameLoadCallbacks();
  void DumpWindowStatusChanges();
  void EnableUseZoomForDSF(v8::Local<v8::Function> callback);
  void EvaluateInWebInspector(int call_id, const std::string& script);
  void EvaluateScriptInIsolatedWorld(int world_id, const std::string& script);
  void ExecCommand(gin::Arguments* args);
  void ForceNextDrawingBufferCreationToFail();
  void ForceNextWebGLContextCreationToFail();
  void ForceRedSelectionColors();
  void GetBluetoothManualChooserEvents(v8::Local<v8::Function> callback);
  void GetManifestThen(v8::Local<v8::Function> callback);
  void InsertStyleSheet(const std::string& source_code);
  void LayoutAndPaintAsync();
  void LayoutAndPaintAsyncThen(v8::Local<v8::Function> callback);
  void LogToStderr(const std::string& output);
  void NotImplemented(const gin::Arguments& args);
  void NotifyDone();
  void OverridePreference(const std::string& key, v8::Local<v8::Value> value);
  void QueueBackNavigation(int how_far_back);
  void QueueForwardNavigation(int how_far_forward);
  void QueueLoad(gin::Arguments* args);
  void QueueLoadingScript(const std::string& script);
  void QueueNonLoadingScript(const std::string& script);
  void QueueReload();
  void RemoveOriginAccessWhitelistEntry(const std::string& source_origin,
                                        const std::string& destination_protocol,
                                        const std::string& destination_host,
                                        bool allow_destination_subdomains);
  void RemoveWebPageOverlay();
  void ResetDeviceLight();
  void ResetTestHelperControllers();
  void ResolveBeforeInstallPromptPromise(int request_id,
                                         const std::string& platform);
  void RunIdleTasks(v8::Local<v8::Function> callback);
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument);
  void SetAcceptLanguages(const std::string& accept_languages);
  void SetAllowDisplayOfInsecureContent(bool allowed);
  void SetAllowFileAccessFromFileURLs(bool allow);
  void SetAllowRunningOfInsecureContent(bool allowed);
  void SetAutoplayAllowed(bool allowed);
  void SetAllowUniversalAccessFromFileURLs(bool allow);
  void SetAlwaysAcceptCookies(bool accept);
  void SetAudioData(const gin::ArrayBufferView& view);
  void SetBackingScaleFactor(double value, v8::Local<v8::Function> callback);
  void SetBluetoothFakeAdapter(const std::string& adapter_name,
                               v8::Local<v8::Function> callback);
  void SetBluetoothManualChooser(bool enable);
  void SetCanOpenWindows();
  void SetCloseRemainingWindowsWhenComplete(gin::Arguments* args);
  void SetColorProfile(const std::string& name,
                       v8::Local<v8::Function> callback);
  void SetCustomPolicyDelegate(gin::Arguments* args);
  void SetCustomTextOutput(const std::string& output);
  void SetDatabaseQuota(int quota);
  void SetDomainRelaxationForbiddenForURLScheme(bool forbidden,
                                                const std::string& scheme);
  void SetGeofencingMockPosition(double latitude, double longitude);
  void SetGeofencingMockProvider(bool service_available);
  void SetImagesAllowed(bool allowed);
  void SetInterceptPostMessage(bool value);
  void SetIsolatedWorldContentSecurityPolicy(int world_id,
                                             const std::string& policy);
  void SetIsolatedWorldSecurityOrigin(int world_id,
                                      v8::Local<v8::Value> origin);
  void SetJavaScriptCanAccessClipboard(bool can_access);
  void SetMIDIAccessorResult(bool result);
  void SetMediaAllowed(bool allowed);
  void SetMockDeviceLight(double value);
  void SetMockDeviceMotion(gin::Arguments* args);
  void SetMockDeviceOrientation(gin::Arguments* args);
  void SetMockScreenOrientation(const std::string& orientation);
  void SetMockSpeechRecognitionError(const std::string& error,
                                     const std::string& message);
  void SetPOSIXLocale(const std::string& locale);
  void SetPageVisibility(const std::string& new_visibility);
  void SetPermission(const std::string& name,
                     const std::string& value,
                     const std::string& origin,
                     const std::string& embedding_origin);
  void SetPluginsAllowed(bool allowed);
  void SetPluginsEnabled(bool enabled);
  void SetPointerLockWillFailSynchronously();
  void SetPointerLockWillRespondAsynchronously();
  void SetPopupBlockingEnabled(bool block_popups);
  void SetPrinting();
  void SetScriptsAllowed(bool allowed);
  void SetShouldStayOnPageAfterHandlingBeforeUnload(bool value);
  void SetStorageAllowed(bool allowed);
  void SetTabKeyCyclesThroughElements(bool tab_key_cycles_through_elements);
  void SetTextDirection(const std::string& direction_name);
  void SetTextSubpixelPositioning(bool value);
  void SetUseMockTheme(bool use);
  void SetViewSourceForFrame(const std::string& name, bool enabled);
  void SetWillSendRequestClearHeader(const std::string& header);
  void SetWindowIsKey(bool value);
  void SetXSSAuditorEnabled(bool enabled);
  void ShowWebInspector(gin::Arguments* args);
  void SimulateWebNotificationClick(const std::string& title, int action_index);
  void SimulateWebNotificationClose(const std::string& title, bool by_user);
  void UseUnfortunateSynchronousResizeMode();
  void WaitForPolicyDelegate();
  void WaitUntilDone();
  void WaitUntilExternalURLLoad();
  void AddMockCredentialManagerError(const std::string& error);
  void AddMockCredentialManagerResponse(const std::string& id,
                                        const std::string& name,
                                        const std::string& avatar,
                                        const std::string& password);
  bool AnimationScheduled();
  bool CallShouldCloseOnWebView();
  bool DisableAutoResizeMode(int new_width, int new_height);
  bool EnableAutoResizeMode(int min_width,
                            int min_height,
                            int max_width,
                            int max_height);
  std::string EvaluateInWebInspectorOverlay(const std::string& script);
  v8::Local<v8::Value> EvaluateScriptInIsolatedWorldAndReturnValue(
      int world_id, const std::string& script);
  bool FindString(const std::string& search_text,
                  const std::vector<std::string>& options_array);
  bool HasCustomPageSizeStyle(int page_index);
  bool InterceptPostMessage();
  bool IsChooserShown();

  bool IsCommandEnabled(const std::string& command);
  std::string PathToLocalResource(const std::string& path);
  std::string PlatformName();
  std::string SelectionAsMarkup();
  std::string TooltipText();

  int WebHistoryItemCount();
  int WindowCount();

  base::WeakPtr<TestRunner> runner_;
  base::WeakPtr<TestRunnerForSpecificView> view_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestRunnerBindings);
};

gin::WrapperInfo TestRunnerBindings::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void TestRunnerBindings::Install(
    base::WeakPtr<TestRunner> test_runner,
    base::WeakPtr<TestRunnerForSpecificView> view_test_runner,
    WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  TestRunnerBindings* wrapped =
      new TestRunnerBindings(test_runner, view_test_runner);
  gin::Handle<TestRunnerBindings> bindings =
      gin::CreateHandle(isolate, wrapped);
  if (bindings.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Value> v8_bindings = bindings.ToV8();

  std::vector<std::string> names;
  names.push_back("testRunner");
  names.push_back("layoutTestController");
  for (size_t i = 0; i < names.size(); ++i)
    global->Set(gin::StringToV8(isolate, names[i].c_str()), v8_bindings);
}

TestRunnerBindings::TestRunnerBindings(
    base::WeakPtr<TestRunner> runner,
    base::WeakPtr<TestRunnerForSpecificView> view_runner)
    : runner_(runner), view_runner_(view_runner) {}

TestRunnerBindings::~TestRunnerBindings() {}

gin::ObjectTemplateBuilder TestRunnerBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<TestRunnerBindings>::GetObjectTemplateBuilder(isolate)
      .SetMethod("abortModal", &TestRunnerBindings::NotImplemented)
      .SetMethod("addDisallowedURL", &TestRunnerBindings::NotImplemented)
      .SetMethod("addMockCredentialManagerError",
                 &TestRunnerBindings::AddMockCredentialManagerError)
      .SetMethod("addMockCredentialManagerResponse",
                 &TestRunnerBindings::AddMockCredentialManagerResponse)
      .SetMethod("addMockSpeechRecognitionResult",
                 &TestRunnerBindings::AddMockSpeechRecognitionResult)
      .SetMethod("addOriginAccessWhitelistEntry",
                 &TestRunnerBindings::AddOriginAccessWhitelistEntry)
      .SetMethod("addWebPageOverlay", &TestRunnerBindings::AddWebPageOverlay)
      .SetMethod("animationScheduled", &TestRunnerBindings::AnimationScheduled)
      .SetMethod("callShouldCloseOnWebView",
                 &TestRunnerBindings::CallShouldCloseOnWebView)
      .SetMethod("capturePixelsAsyncThen",
                 &TestRunnerBindings::CapturePixelsAsyncThen)
      .SetMethod("clearAllDatabases", &TestRunnerBindings::ClearAllDatabases)
      .SetMethod("clearBackForwardList", &TestRunnerBindings::NotImplemented)
      .SetMethod("clearGeofencingMockProvider",
                 &TestRunnerBindings::ClearGeofencingMockProvider)
      .SetMethod("clearPrinting", &TestRunnerBindings::ClearPrinting)
      .SetMethod("closeWebInspector", &TestRunnerBindings::CloseWebInspector)
      .SetMethod("copyImageAtAndCapturePixelsAsyncThen",
                 &TestRunnerBindings::CopyImageAtAndCapturePixelsAsyncThen)
      .SetMethod("didAcquirePointerLock",
                 &TestRunnerBindings::DidAcquirePointerLock)
      .SetMethod("didLosePointerLock", &TestRunnerBindings::DidLosePointerLock)
      .SetMethod("didNotAcquirePointerLock",
                 &TestRunnerBindings::DidNotAcquirePointerLock)
      .SetMethod("disableAutoResizeMode",
                 &TestRunnerBindings::DisableAutoResizeMode)
      .SetMethod("disableMockScreenOrientation",
                 &TestRunnerBindings::DisableMockScreenOrientation)
      .SetMethod("dispatchBeforeInstallPromptEvent",
                 &TestRunnerBindings::DispatchBeforeInstallPromptEvent)
      .SetMethod("dumpAsMarkup", &TestRunnerBindings::DumpAsMarkup)
      .SetMethod("dumpAsText", &TestRunnerBindings::DumpAsText)
      .SetMethod("dumpAsTextWithPixelResults",
                 &TestRunnerBindings::DumpAsTextWithPixelResults)
      .SetMethod("dumpBackForwardList",
                 &TestRunnerBindings::DumpBackForwardList)
      .SetMethod("dumpChildFrameScrollPositions",
                 &TestRunnerBindings::DumpChildFrameScrollPositions)
      .SetMethod("dumpChildFramesAsMarkup",
                 &TestRunnerBindings::DumpChildFramesAsMarkup)
      .SetMethod("dumpChildFramesAsText",
                 &TestRunnerBindings::DumpChildFramesAsText)
      .SetMethod("dumpCreateView", &TestRunnerBindings::DumpCreateView)
      .SetMethod("dumpDatabaseCallbacks", &TestRunnerBindings::NotImplemented)
      .SetMethod("dumpDragImage", &TestRunnerBindings::DumpDragImage)
      .SetMethod("dumpEditingCallbacks",
                 &TestRunnerBindings::DumpEditingCallbacks)
      .SetMethod("dumpFrameLoadCallbacks",
                 &TestRunnerBindings::DumpFrameLoadCallbacks)
      .SetMethod("dumpIconChanges", &TestRunnerBindings::DumpIconChanges)
      .SetMethod("dumpNavigationPolicy",
                 &TestRunnerBindings::DumpNavigationPolicy)
      .SetMethod("dumpPageImportanceSignals",
                 &TestRunnerBindings::DumpPageImportanceSignals)
      .SetMethod("dumpPermissionClientCallbacks",
                 &TestRunnerBindings::DumpPermissionClientCallbacks)
      .SetMethod("dumpPingLoaderCallbacks",
                 &TestRunnerBindings::DumpPingLoaderCallbacks)
      .SetMethod("dumpResourceLoadCallbacks",
                 &TestRunnerBindings::DumpResourceLoadCallbacks)
      .SetMethod("dumpResourceRequestPriorities",
                 &TestRunnerBindings::DumpResourceRequestPriorities)
      .SetMethod("dumpResourceResponseMIMETypes",
                 &TestRunnerBindings::DumpResourceResponseMIMETypes)
      .SetMethod("dumpSelectionRect", &TestRunnerBindings::DumpSelectionRect)
      .SetMethod("dumpSpellCheckCallbacks",
                 &TestRunnerBindings::DumpSpellCheckCallbacks)

      // Used at fast/dom/assign-to-window-status.html
      .SetMethod("dumpStatusCallbacks",
                 &TestRunnerBindings::DumpWindowStatusChanges)
      .SetMethod("dumpTitleChanges", &TestRunnerBindings::DumpTitleChanges)
      .SetMethod("dumpUserGestureInFrameLoadCallbacks",
                 &TestRunnerBindings::DumpUserGestureInFrameLoadCallbacks)
      .SetMethod("enableAutoResizeMode",
                 &TestRunnerBindings::EnableAutoResizeMode)
      .SetMethod("enableUseZoomForDSF",
                 &TestRunnerBindings::EnableUseZoomForDSF)
      .SetMethod("evaluateInWebInspector",
                 &TestRunnerBindings::EvaluateInWebInspector)
      .SetMethod("evaluateInWebInspectorOverlay",
                 &TestRunnerBindings::EvaluateInWebInspectorOverlay)
      .SetMethod("evaluateScriptInIsolatedWorld",
                 &TestRunnerBindings::EvaluateScriptInIsolatedWorld)
      .SetMethod(
          "evaluateScriptInIsolatedWorldAndReturnValue",
          &TestRunnerBindings::EvaluateScriptInIsolatedWorldAndReturnValue)
      .SetMethod("execCommand", &TestRunnerBindings::ExecCommand)
      .SetMethod("findString", &TestRunnerBindings::FindString)
      .SetMethod("forceNextDrawingBufferCreationToFail",
                 &TestRunnerBindings::ForceNextDrawingBufferCreationToFail)
      .SetMethod("forceNextWebGLContextCreationToFail",
                 &TestRunnerBindings::ForceNextWebGLContextCreationToFail)
      .SetMethod("forceRedSelectionColors",
                 &TestRunnerBindings::ForceRedSelectionColors)

      // The Bluetooth functions are specified at
      // https://webbluetoothcg.github.io/web-bluetooth/tests/.
      .SetMethod("getBluetoothManualChooserEvents",
                 &TestRunnerBindings::GetBluetoothManualChooserEvents)
      .SetMethod("getManifestThen", &TestRunnerBindings::GetManifestThen)
      .SetMethod("hasCustomPageSizeStyle",
                 &TestRunnerBindings::HasCustomPageSizeStyle)
      .SetMethod("insertStyleSheet", &TestRunnerBindings::InsertStyleSheet)
      .SetProperty("interceptPostMessage",
                   &TestRunnerBindings::InterceptPostMessage,
                   &TestRunnerBindings::SetInterceptPostMessage)
      .SetMethod("isChooserShown", &TestRunnerBindings::IsChooserShown)
      .SetMethod("isCommandEnabled", &TestRunnerBindings::IsCommandEnabled)
      .SetMethod("keepWebHistory", &TestRunnerBindings::NotImplemented)
      .SetMethod("layoutAndPaintAsync",
                 &TestRunnerBindings::LayoutAndPaintAsync)
      .SetMethod("layoutAndPaintAsyncThen",
                 &TestRunnerBindings::LayoutAndPaintAsyncThen)
      .SetMethod("logToStderr", &TestRunnerBindings::LogToStderr)
      .SetMethod("notifyDone", &TestRunnerBindings::NotifyDone)
      .SetMethod("overridePreference", &TestRunnerBindings::OverridePreference)
      .SetMethod("pathToLocalResource",
                 &TestRunnerBindings::PathToLocalResource)
      .SetProperty("platformName", &TestRunnerBindings::PlatformName)
      .SetMethod("queueBackNavigation",
                 &TestRunnerBindings::QueueBackNavigation)
      .SetMethod("queueForwardNavigation",
                 &TestRunnerBindings::QueueForwardNavigation)
      .SetMethod("queueLoad", &TestRunnerBindings::QueueLoad)
      .SetMethod("queueLoadingScript", &TestRunnerBindings::QueueLoadingScript)
      .SetMethod("queueNonLoadingScript",
                 &TestRunnerBindings::QueueNonLoadingScript)
      .SetMethod("queueReload", &TestRunnerBindings::QueueReload)
      .SetMethod("removeOriginAccessWhitelistEntry",
                 &TestRunnerBindings::RemoveOriginAccessWhitelistEntry)
      .SetMethod("removeWebPageOverlay",
                 &TestRunnerBindings::RemoveWebPageOverlay)
      .SetMethod("resetDeviceLight", &TestRunnerBindings::ResetDeviceLight)
      .SetMethod("resetTestHelperControllers",
                 &TestRunnerBindings::ResetTestHelperControllers)
      .SetMethod("resolveBeforeInstallPromptPromise",
                 &TestRunnerBindings::ResolveBeforeInstallPromptPromise)
      .SetMethod("runIdleTasks",
                 &TestRunnerBindings::RunIdleTasks)
      .SetMethod("selectionAsMarkup", &TestRunnerBindings::SelectionAsMarkup)

      // The Bluetooth functions are specified at
      // https://webbluetoothcg.github.io/web-bluetooth/tests/.
      .SetMethod("sendBluetoothManualChooserEvent",
                 &TestRunnerBindings::SendBluetoothManualChooserEvent)
      .SetMethod("setAcceptLanguages", &TestRunnerBindings::SetAcceptLanguages)
      .SetMethod("setAllowDisplayOfInsecureContent",
                 &TestRunnerBindings::SetAllowDisplayOfInsecureContent)
      .SetMethod("setAllowFileAccessFromFileURLs",
                 &TestRunnerBindings::SetAllowFileAccessFromFileURLs)
      .SetMethod("setAllowRunningOfInsecureContent",
                 &TestRunnerBindings::SetAllowRunningOfInsecureContent)
      .SetMethod("setAutoplayAllowed",
                 &TestRunnerBindings::SetAutoplayAllowed)
      .SetMethod("setAllowUniversalAccessFromFileURLs",
                 &TestRunnerBindings::SetAllowUniversalAccessFromFileURLs)
      .SetMethod("setAlwaysAcceptCookies",
                 &TestRunnerBindings::SetAlwaysAcceptCookies)
      .SetMethod("setAudioData", &TestRunnerBindings::SetAudioData)
      .SetMethod("setBackingScaleFactor",
                 &TestRunnerBindings::SetBackingScaleFactor)
      // The Bluetooth functions are specified at
      // https://webbluetoothcg.github.io/web-bluetooth/tests/.
      .SetMethod("setBluetoothFakeAdapter",
                 &TestRunnerBindings::SetBluetoothFakeAdapter)
      .SetMethod("setBluetoothManualChooser",
                 &TestRunnerBindings::SetBluetoothManualChooser)
      .SetMethod("setCallCloseOnWebViews", &TestRunnerBindings::NotImplemented)
      .SetMethod("setCanOpenWindows", &TestRunnerBindings::SetCanOpenWindows)
      .SetMethod("setCloseRemainingWindowsWhenComplete",
                 &TestRunnerBindings::SetCloseRemainingWindowsWhenComplete)
      .SetMethod("setColorProfile", &TestRunnerBindings::SetColorProfile)
      .SetMethod("setCustomPolicyDelegate",
                 &TestRunnerBindings::SetCustomPolicyDelegate)
      .SetMethod("setCustomTextOutput",
                 &TestRunnerBindings::SetCustomTextOutput)
      .SetMethod("setDatabaseQuota", &TestRunnerBindings::SetDatabaseQuota)
      .SetMethod("setDomainRelaxationForbiddenForURLScheme",
                 &TestRunnerBindings::SetDomainRelaxationForbiddenForURLScheme)
      .SetMethod("setGeofencingMockPosition",
                 &TestRunnerBindings::SetGeofencingMockPosition)
      .SetMethod("setGeofencingMockProvider",
                 &TestRunnerBindings::SetGeofencingMockProvider)
      .SetMethod("setIconDatabaseEnabled", &TestRunnerBindings::NotImplemented)
      .SetMethod("setImagesAllowed", &TestRunnerBindings::SetImagesAllowed)
      .SetMethod("setIsolatedWorldContentSecurityPolicy",
                 &TestRunnerBindings::SetIsolatedWorldContentSecurityPolicy)
      .SetMethod("setIsolatedWorldSecurityOrigin",
                 &TestRunnerBindings::SetIsolatedWorldSecurityOrigin)
      .SetMethod("setJavaScriptCanAccessClipboard",
                 &TestRunnerBindings::SetJavaScriptCanAccessClipboard)
      .SetMethod("setMIDIAccessorResult",
                 &TestRunnerBindings::SetMIDIAccessorResult)
      .SetMethod("setMainFrameIsFirstResponder",
                 &TestRunnerBindings::NotImplemented)
      .SetMethod("setMediaAllowed", &TestRunnerBindings::SetMediaAllowed)
      .SetMethod("setMockDeviceLight", &TestRunnerBindings::SetMockDeviceLight)
      .SetMethod("setMockDeviceMotion",
                 &TestRunnerBindings::SetMockDeviceMotion)
      .SetMethod("setMockDeviceOrientation",
                 &TestRunnerBindings::SetMockDeviceOrientation)
      .SetMethod("setMockScreenOrientation",
                 &TestRunnerBindings::SetMockScreenOrientation)
      .SetMethod("setMockSpeechRecognitionError",
                 &TestRunnerBindings::SetMockSpeechRecognitionError)
      .SetMethod("setPOSIXLocale", &TestRunnerBindings::SetPOSIXLocale)
      .SetMethod("setPageVisibility", &TestRunnerBindings::SetPageVisibility)
      .SetMethod("setPermission", &TestRunnerBindings::SetPermission)
      .SetMethod("setPluginsAllowed", &TestRunnerBindings::SetPluginsAllowed)
      .SetMethod("setPluginsEnabled", &TestRunnerBindings::SetPluginsEnabled)
      .SetMethod("setPointerLockWillFailSynchronously",
                 &TestRunnerBindings::SetPointerLockWillFailSynchronously)
      .SetMethod("setPointerLockWillRespondAsynchronously",
                 &TestRunnerBindings::SetPointerLockWillRespondAsynchronously)
      .SetMethod("setPopupBlockingEnabled",
                 &TestRunnerBindings::SetPopupBlockingEnabled)
      .SetMethod("setPrinting", &TestRunnerBindings::SetPrinting)
      .SetMethod("setScriptsAllowed", &TestRunnerBindings::SetScriptsAllowed)
      .SetMethod("setScrollbarPolicy", &TestRunnerBindings::NotImplemented)
      .SetMethod(
          "setShouldStayOnPageAfterHandlingBeforeUnload",
          &TestRunnerBindings::SetShouldStayOnPageAfterHandlingBeforeUnload)
      .SetMethod("setStorageAllowed", &TestRunnerBindings::SetStorageAllowed)
      .SetMethod("setTabKeyCyclesThroughElements",
                 &TestRunnerBindings::SetTabKeyCyclesThroughElements)
      .SetMethod("setTextDirection", &TestRunnerBindings::SetTextDirection)
      .SetMethod("setTextSubpixelPositioning",
                 &TestRunnerBindings::SetTextSubpixelPositioning)
      .SetMethod("setUseDashboardCompatibilityMode",
                 &TestRunnerBindings::NotImplemented)
      .SetMethod("setUseMockTheme", &TestRunnerBindings::SetUseMockTheme)
      .SetMethod("setViewSourceForFrame",
                 &TestRunnerBindings::SetViewSourceForFrame)
      .SetMethod("setWillSendRequestClearHeader",
                 &TestRunnerBindings::SetWillSendRequestClearHeader)
      .SetMethod("setWindowIsKey", &TestRunnerBindings::SetWindowIsKey)
      .SetMethod("setXSSAuditorEnabled",
                 &TestRunnerBindings::SetXSSAuditorEnabled)
      .SetMethod("showWebInspector", &TestRunnerBindings::ShowWebInspector)
      .SetMethod("simulateWebNotificationClick",
                 &TestRunnerBindings::SimulateWebNotificationClick)
      .SetMethod("simulateWebNotificationClose",
                 &TestRunnerBindings::SimulateWebNotificationClose)
      .SetProperty("tooltipText", &TestRunnerBindings::TooltipText)
      .SetMethod("useUnfortunateSynchronousResizeMode",
                 &TestRunnerBindings::UseUnfortunateSynchronousResizeMode)
      .SetMethod("waitForPolicyDelegate",
                 &TestRunnerBindings::WaitForPolicyDelegate)
      .SetMethod("waitUntilDone", &TestRunnerBindings::WaitUntilDone)
      .SetMethod("waitUntilExternalURLLoad",
                 &TestRunnerBindings::WaitUntilExternalURLLoad)

      // webHistoryItemCount is used by tests in LayoutTests\http\tests\history
      .SetProperty("webHistoryItemCount",
                   &TestRunnerBindings::WebHistoryItemCount)
      .SetMethod("windowCount", &TestRunnerBindings::WindowCount);
}

void TestRunnerBindings::LogToStderr(const std::string& output) {
  LOG(ERROR) << output;
}

void TestRunnerBindings::NotifyDone() {
  if (runner_)
    runner_->NotifyDone();
}

void TestRunnerBindings::WaitUntilDone() {
  if (runner_)
    runner_->WaitUntilDone();
}

void TestRunnerBindings::QueueBackNavigation(int how_far_back) {
  if (runner_)
    runner_->QueueBackNavigation(how_far_back);
}

void TestRunnerBindings::QueueForwardNavigation(int how_far_forward) {
  if (runner_)
    runner_->QueueForwardNavigation(how_far_forward);
}

void TestRunnerBindings::QueueReload() {
  if (runner_)
    runner_->QueueReload();
}

void TestRunnerBindings::QueueLoadingScript(const std::string& script) {
  if (runner_)
    runner_->QueueLoadingScript(script);
}

void TestRunnerBindings::QueueNonLoadingScript(const std::string& script) {
  if (runner_)
    runner_->QueueNonLoadingScript(script);
}

void TestRunnerBindings::QueueLoad(gin::Arguments* args) {
  if (runner_) {
    std::string url;
    std::string target;
    args->GetNext(&url);
    args->GetNext(&target);
    runner_->QueueLoad(url, target);
  }
}

void TestRunnerBindings::SetCustomPolicyDelegate(gin::Arguments* args) {
  if (runner_)
    runner_->SetCustomPolicyDelegate(args);
}

void TestRunnerBindings::WaitForPolicyDelegate() {
  if (runner_)
    runner_->WaitForPolicyDelegate();
}

int TestRunnerBindings::WindowCount() {
  if (runner_)
    return runner_->WindowCount();
  return 0;
}

void TestRunnerBindings::SetCloseRemainingWindowsWhenComplete(
    gin::Arguments* args) {
  if (!runner_)
    return;

  // In the original implementation, nothing happens if the argument is
  // ommitted.
  bool close_remaining_windows = false;
  if (args->GetNext(&close_remaining_windows))
    runner_->SetCloseRemainingWindowsWhenComplete(close_remaining_windows);
}

void TestRunnerBindings::ResetTestHelperControllers() {
  if (runner_)
    runner_->ResetTestHelperControllers();
}

void TestRunnerBindings::SetTabKeyCyclesThroughElements(
    bool tab_key_cycles_through_elements) {
  if (view_runner_)
    view_runner_->SetTabKeyCyclesThroughElements(
        tab_key_cycles_through_elements);
}

void TestRunnerBindings::ExecCommand(gin::Arguments* args) {
  if (view_runner_)
    view_runner_->ExecCommand(args);
}

bool TestRunnerBindings::IsCommandEnabled(const std::string& command) {
  if (view_runner_)
    return view_runner_->IsCommandEnabled(command);
  return false;
}

bool TestRunnerBindings::CallShouldCloseOnWebView() {
  if (view_runner_)
    return view_runner_->CallShouldCloseOnWebView();
  return false;
}

void TestRunnerBindings::SetDomainRelaxationForbiddenForURLScheme(
    bool forbidden, const std::string& scheme) {
  if (view_runner_)
    view_runner_->SetDomainRelaxationForbiddenForURLScheme(forbidden, scheme);
}

v8::Local<v8::Value>
TestRunnerBindings::EvaluateScriptInIsolatedWorldAndReturnValue(
    int world_id, const std::string& script) {
  if (!view_runner_ || world_id <= 0 || world_id >= (1 << 29))
    return v8::Local<v8::Value>();
  return view_runner_->EvaluateScriptInIsolatedWorldAndReturnValue(world_id,
                                                                   script);
}

void TestRunnerBindings::EvaluateScriptInIsolatedWorld(
    int world_id, const std::string& script) {
  if (view_runner_ && world_id > 0 && world_id < (1 << 29))
    view_runner_->EvaluateScriptInIsolatedWorld(world_id, script);
}

void TestRunnerBindings::SetIsolatedWorldSecurityOrigin(
    int world_id, v8::Local<v8::Value> origin) {
  if (view_runner_)
    view_runner_->SetIsolatedWorldSecurityOrigin(world_id, origin);
}

void TestRunnerBindings::SetIsolatedWorldContentSecurityPolicy(
    int world_id, const std::string& policy) {
  if (view_runner_)
    view_runner_->SetIsolatedWorldContentSecurityPolicy(world_id, policy);
}

void TestRunnerBindings::AddOriginAccessWhitelistEntry(
    const std::string& source_origin,
    const std::string& destination_protocol,
    const std::string& destination_host,
    bool allow_destination_subdomains) {
  if (runner_) {
    runner_->AddOriginAccessWhitelistEntry(source_origin,
                                           destination_protocol,
                                           destination_host,
                                           allow_destination_subdomains);
  }
}

void TestRunnerBindings::RemoveOriginAccessWhitelistEntry(
    const std::string& source_origin,
    const std::string& destination_protocol,
    const std::string& destination_host,
    bool allow_destination_subdomains) {
  if (runner_) {
    runner_->RemoveOriginAccessWhitelistEntry(source_origin,
                                              destination_protocol,
                                              destination_host,
                                              allow_destination_subdomains);
  }
}

bool TestRunnerBindings::HasCustomPageSizeStyle(int page_index) {
  if (view_runner_)
    return view_runner_->HasCustomPageSizeStyle(page_index);
  return false;
}

void TestRunnerBindings::ForceRedSelectionColors() {
  if (view_runner_)
    view_runner_->ForceRedSelectionColors();
}

void TestRunnerBindings::InsertStyleSheet(const std::string& source_code) {
  if (runner_)
    runner_->InsertStyleSheet(source_code);
}

bool TestRunnerBindings::FindString(
    const std::string& search_text,
    const std::vector<std::string>& options_array) {
  if (view_runner_)
    return view_runner_->FindString(search_text, options_array);
  return false;
}

std::string TestRunnerBindings::SelectionAsMarkup() {
  if (view_runner_)
    return view_runner_->SelectionAsMarkup();
  return std::string();
}

void TestRunnerBindings::SetTextSubpixelPositioning(bool value) {
  if (runner_)
    runner_->SetTextSubpixelPositioning(value);
}

void TestRunnerBindings::SetPageVisibility(const std::string& new_visibility) {
  if (view_runner_)
    view_runner_->SetPageVisibility(new_visibility);
}

void TestRunnerBindings::SetTextDirection(const std::string& direction_name) {
  if (view_runner_)
    view_runner_->SetTextDirection(direction_name);
}

void TestRunnerBindings::UseUnfortunateSynchronousResizeMode() {
  if (runner_)
    runner_->UseUnfortunateSynchronousResizeMode();
}

bool TestRunnerBindings::EnableAutoResizeMode(int min_width,
                                              int min_height,
                                              int max_width,
                                              int max_height) {
  if (runner_) {
    return runner_->EnableAutoResizeMode(min_width, min_height,
                                         max_width, max_height);
  }
  return false;
}

bool TestRunnerBindings::DisableAutoResizeMode(int new_width, int new_height) {
  if (runner_)
    return runner_->DisableAutoResizeMode(new_width, new_height);
  return false;
}

void TestRunnerBindings::SetMockDeviceLight(double value) {
  if (!runner_)
    return;
  runner_->SetMockDeviceLight(value);
}

void TestRunnerBindings::ResetDeviceLight() {
  if (runner_)
    runner_->ResetDeviceLight();
}

void TestRunnerBindings::SetMockDeviceMotion(gin::Arguments* args) {
  if (!runner_)
    return;

  bool has_acceleration_x;
  double acceleration_x;
  bool has_acceleration_y;
  double acceleration_y;
  bool has_acceleration_z;
  double acceleration_z;
  bool has_acceleration_including_gravity_x;
  double acceleration_including_gravity_x;
  bool has_acceleration_including_gravity_y;
  double acceleration_including_gravity_y;
  bool has_acceleration_including_gravity_z;
  double acceleration_including_gravity_z;
  bool has_rotation_rate_alpha;
  double rotation_rate_alpha;
  bool has_rotation_rate_beta;
  double rotation_rate_beta;
  bool has_rotation_rate_gamma;
  double rotation_rate_gamma;
  double interval;

  args->GetNext(&has_acceleration_x);
  args->GetNext(& acceleration_x);
  args->GetNext(&has_acceleration_y);
  args->GetNext(& acceleration_y);
  args->GetNext(&has_acceleration_z);
  args->GetNext(& acceleration_z);
  args->GetNext(&has_acceleration_including_gravity_x);
  args->GetNext(& acceleration_including_gravity_x);
  args->GetNext(&has_acceleration_including_gravity_y);
  args->GetNext(& acceleration_including_gravity_y);
  args->GetNext(&has_acceleration_including_gravity_z);
  args->GetNext(& acceleration_including_gravity_z);
  args->GetNext(&has_rotation_rate_alpha);
  args->GetNext(& rotation_rate_alpha);
  args->GetNext(&has_rotation_rate_beta);
  args->GetNext(& rotation_rate_beta);
  args->GetNext(&has_rotation_rate_gamma);
  args->GetNext(& rotation_rate_gamma);
  args->GetNext(& interval);

  runner_->SetMockDeviceMotion(has_acceleration_x, acceleration_x,
                               has_acceleration_y, acceleration_y,
                               has_acceleration_z, acceleration_z,
                               has_acceleration_including_gravity_x,
                               acceleration_including_gravity_x,
                               has_acceleration_including_gravity_y,
                               acceleration_including_gravity_y,
                               has_acceleration_including_gravity_z,
                               acceleration_including_gravity_z,
                               has_rotation_rate_alpha,
                               rotation_rate_alpha,
                               has_rotation_rate_beta,
                               rotation_rate_beta,
                               has_rotation_rate_gamma,
                               rotation_rate_gamma,
                               interval);
}

void TestRunnerBindings::SetMockDeviceOrientation(gin::Arguments* args) {
  if (!runner_)
    return;

  bool has_alpha = false;
  double alpha = 0.0;
  bool has_beta = false;
  double beta = 0.0;
  bool has_gamma = false;
  double gamma = 0.0;
  bool absolute = false;

  args->GetNext(&has_alpha);
  args->GetNext(&alpha);
  args->GetNext(&has_beta);
  args->GetNext(&beta);
  args->GetNext(&has_gamma);
  args->GetNext(&gamma);
  args->GetNext(&absolute);

  runner_->SetMockDeviceOrientation(has_alpha, alpha,
                                    has_beta, beta,
                                    has_gamma, gamma,
                                    absolute);
}

void TestRunnerBindings::SetMockScreenOrientation(
    const std::string& orientation) {
  if (!runner_)
    return;

  runner_->SetMockScreenOrientation(orientation);
}

void TestRunnerBindings::DisableMockScreenOrientation() {
  if (runner_)
    runner_->DisableMockScreenOrientation();
}

void TestRunnerBindings::DidAcquirePointerLock() {
  if (view_runner_)
    view_runner_->DidAcquirePointerLock();
}

void TestRunnerBindings::DidNotAcquirePointerLock() {
  if (view_runner_)
    view_runner_->DidNotAcquirePointerLock();
}

void TestRunnerBindings::DidLosePointerLock() {
  if (view_runner_)
    view_runner_->DidLosePointerLock();
}

void TestRunnerBindings::SetPointerLockWillFailSynchronously() {
  if (view_runner_)
    view_runner_->SetPointerLockWillFailSynchronously();
}

void TestRunnerBindings::SetPointerLockWillRespondAsynchronously() {
  if (view_runner_)
    view_runner_->SetPointerLockWillRespondAsynchronously();
}

void TestRunnerBindings::SetPopupBlockingEnabled(bool block_popups) {
  if (runner_)
    runner_->SetPopupBlockingEnabled(block_popups);
}

void TestRunnerBindings::SetJavaScriptCanAccessClipboard(bool can_access) {
  if (runner_)
    runner_->SetJavaScriptCanAccessClipboard(can_access);
}

void TestRunnerBindings::SetXSSAuditorEnabled(bool enabled) {
  if (runner_)
    runner_->SetXSSAuditorEnabled(enabled);
}

void TestRunnerBindings::SetAllowUniversalAccessFromFileURLs(bool allow) {
  if (runner_)
    runner_->SetAllowUniversalAccessFromFileURLs(allow);
}

void TestRunnerBindings::SetAllowFileAccessFromFileURLs(bool allow) {
  if (runner_)
    runner_->SetAllowFileAccessFromFileURLs(allow);
}

void TestRunnerBindings::OverridePreference(const std::string& key,
                                            v8::Local<v8::Value> value) {
  if (runner_)
    runner_->OverridePreference(key, value);
}

void TestRunnerBindings::SetAcceptLanguages(
    const std::string& accept_languages) {
  if (!runner_)
    return;

  runner_->SetAcceptLanguages(accept_languages);
}

void TestRunnerBindings::SetPluginsEnabled(bool enabled) {
  if (runner_)
    runner_->SetPluginsEnabled(enabled);
}

bool TestRunnerBindings::AnimationScheduled() {
  if (runner_)
    return runner_->GetAnimationScheduled();
  else
    return false;
}

void TestRunnerBindings::DumpEditingCallbacks() {
  if (runner_)
    runner_->DumpEditingCallbacks();
}

void TestRunnerBindings::DumpAsMarkup() {
  if (runner_)
    runner_->DumpAsMarkup();
}

void TestRunnerBindings::DumpAsText() {
  if (runner_)
    runner_->DumpAsText();
}

void TestRunnerBindings::DumpAsTextWithPixelResults() {
  if (runner_)
    runner_->DumpAsTextWithPixelResults();
}

void TestRunnerBindings::DumpChildFrameScrollPositions() {
  if (runner_)
    runner_->DumpChildFrameScrollPositions();
}

void TestRunnerBindings::DumpChildFramesAsText() {
  if (runner_)
    runner_->DumpChildFramesAsText();
}

void TestRunnerBindings::DumpChildFramesAsMarkup() {
  if (runner_)
    runner_->DumpChildFramesAsMarkup();
}

void TestRunnerBindings::DumpIconChanges() {
  if (runner_)
    runner_->DumpIconChanges();
}

void TestRunnerBindings::SetAudioData(const gin::ArrayBufferView& view) {
  if (runner_)
    runner_->SetAudioData(view);
}

void TestRunnerBindings::DumpFrameLoadCallbacks() {
  if (runner_)
    runner_->DumpFrameLoadCallbacks();
}

void TestRunnerBindings::DumpPingLoaderCallbacks() {
  if (runner_)
    runner_->DumpPingLoaderCallbacks();
}

void TestRunnerBindings::DumpUserGestureInFrameLoadCallbacks() {
  if (runner_)
    runner_->DumpUserGestureInFrameLoadCallbacks();
}

void TestRunnerBindings::DumpTitleChanges() {
  if (runner_)
    runner_->DumpTitleChanges();
}

void TestRunnerBindings::DumpCreateView() {
  if (runner_)
    runner_->DumpCreateView();
}

void TestRunnerBindings::SetCanOpenWindows() {
  if (runner_)
    runner_->SetCanOpenWindows();
}

void TestRunnerBindings::DumpResourceLoadCallbacks() {
  if (runner_)
    runner_->DumpResourceLoadCallbacks();
}

void TestRunnerBindings::DumpResourceResponseMIMETypes() {
  if (runner_)
    runner_->DumpResourceResponseMIMETypes();
}

void TestRunnerBindings::SetImagesAllowed(bool allowed) {
  if (runner_)
    runner_->SetImagesAllowed(allowed);
}

void TestRunnerBindings::SetMediaAllowed(bool allowed) {
  if (runner_)
    runner_->SetMediaAllowed(allowed);
}

void TestRunnerBindings::SetScriptsAllowed(bool allowed) {
  if (runner_)
    runner_->SetScriptsAllowed(allowed);
}

void TestRunnerBindings::SetStorageAllowed(bool allowed) {
  if (runner_)
    runner_->SetStorageAllowed(allowed);
}

void TestRunnerBindings::SetPluginsAllowed(bool allowed) {
  if (runner_)
    runner_->SetPluginsAllowed(allowed);
}

void TestRunnerBindings::SetAllowDisplayOfInsecureContent(bool allowed) {
  if (runner_)
    runner_->SetAllowDisplayOfInsecureContent(allowed);
}

void TestRunnerBindings::SetAllowRunningOfInsecureContent(bool allowed) {
  if (runner_)
    runner_->SetAllowRunningOfInsecureContent(allowed);
}

void TestRunnerBindings::SetAutoplayAllowed(bool allowed) {
  if (runner_)
    runner_->SetAutoplayAllowed(allowed);
}

void TestRunnerBindings::DumpPermissionClientCallbacks() {
  if (runner_)
    runner_->DumpPermissionClientCallbacks();
}

void TestRunnerBindings::DumpWindowStatusChanges() {
  if (runner_)
    runner_->DumpWindowStatusChanges();
}

void TestRunnerBindings::DumpSpellCheckCallbacks() {
  if (runner_)
    runner_->DumpSpellCheckCallbacks();
}

void TestRunnerBindings::DumpBackForwardList() {
  if (runner_)
    runner_->DumpBackForwardList();
}

void TestRunnerBindings::DumpSelectionRect() {
  if (runner_)
    runner_->DumpSelectionRect();
}

void TestRunnerBindings::SetPrinting() {
  if (runner_)
    runner_->SetPrinting();
}

void TestRunnerBindings::ClearPrinting() {
  if (runner_)
    runner_->ClearPrinting();
}

void TestRunnerBindings::SetShouldStayOnPageAfterHandlingBeforeUnload(
    bool value) {
  if (runner_)
    runner_->SetShouldStayOnPageAfterHandlingBeforeUnload(value);
}

void TestRunnerBindings::SetWillSendRequestClearHeader(
    const std::string& header) {
  if (runner_)
    runner_->SetWillSendRequestClearHeader(header);
}

void TestRunnerBindings::DumpResourceRequestPriorities() {
  if (runner_)
    runner_->DumpResourceRequestPriorities();
}

void TestRunnerBindings::SetUseMockTheme(bool use) {
  if (runner_)
    runner_->SetUseMockTheme(use);
}

void TestRunnerBindings::WaitUntilExternalURLLoad() {
  if (runner_)
    runner_->WaitUntilExternalURLLoad();
}

void TestRunnerBindings::DumpDragImage() {
  if (runner_)
    runner_->DumpDragImage();
}

void TestRunnerBindings::DumpNavigationPolicy() {
  if (runner_)
    runner_->DumpNavigationPolicy();
}

void TestRunnerBindings::DumpPageImportanceSignals() {
  if (view_runner_)
    view_runner_->DumpPageImportanceSignals();
}

void TestRunnerBindings::ShowWebInspector(gin::Arguments* args) {
  if (runner_) {
    std::string settings;
    args->GetNext(&settings);
    std::string frontend_url;
    args->GetNext(&frontend_url);
    runner_->ShowWebInspector(settings, frontend_url);
  }
}

void TestRunnerBindings::CloseWebInspector() {
  if (runner_)
    runner_->CloseWebInspector();
}

bool TestRunnerBindings::IsChooserShown() {
  if (runner_)
    return runner_->IsChooserShown();
  return false;
}

void TestRunnerBindings::EvaluateInWebInspector(int call_id,
                                                const std::string& script) {
  if (runner_)
    runner_->EvaluateInWebInspector(call_id, script);
}

std::string TestRunnerBindings::EvaluateInWebInspectorOverlay(
    const std::string& script) {
  if (runner_)
    return runner_->EvaluateInWebInspectorOverlay(script);

  return std::string();
}

void TestRunnerBindings::ClearAllDatabases() {
  if (runner_)
    runner_->ClearAllDatabases();
}

void TestRunnerBindings::SetDatabaseQuota(int quota) {
  if (runner_)
    runner_->SetDatabaseQuota(quota);
}

void TestRunnerBindings::SetAlwaysAcceptCookies(bool accept) {
  if (runner_)
    runner_->SetAlwaysAcceptCookies(accept);
}

void TestRunnerBindings::SetWindowIsKey(bool value) {
  if (view_runner_)
    view_runner_->SetWindowIsKey(value);
}

std::string TestRunnerBindings::PathToLocalResource(const std::string& path) {
  if (runner_)
    return runner_->PathToLocalResource(path);
  return std::string();
}

void TestRunnerBindings::SetBackingScaleFactor(
    double value, v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->SetBackingScaleFactor(value, callback);
}

void TestRunnerBindings::EnableUseZoomForDSF(
    v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->EnableUseZoomForDSF(callback);
}

void TestRunnerBindings::SetColorProfile(
    const std::string& name, v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->SetColorProfile(name, callback);
}

void TestRunnerBindings::SetBluetoothFakeAdapter(
    const std::string& adapter_name,
    v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->SetBluetoothFakeAdapter(adapter_name, callback);
}

void TestRunnerBindings::SetBluetoothManualChooser(bool enable) {
  if (view_runner_)
    view_runner_->SetBluetoothManualChooser(enable);
}

void TestRunnerBindings::GetBluetoothManualChooserEvents(
    v8::Local<v8::Function> callback) {
  if (view_runner_)
    return view_runner_->GetBluetoothManualChooserEvents(callback);
}

void TestRunnerBindings::SendBluetoothManualChooserEvent(
    const std::string& event,
    const std::string& argument) {
  if (view_runner_)
    view_runner_->SendBluetoothManualChooserEvent(event, argument);
}

void TestRunnerBindings::SetPOSIXLocale(const std::string& locale) {
  if (runner_)
    runner_->SetPOSIXLocale(locale);
}

void TestRunnerBindings::SetMIDIAccessorResult(bool result) {
  if (runner_)
    runner_->SetMIDIAccessorResult(result);
}

void TestRunnerBindings::SimulateWebNotificationClick(const std::string& title,
                                                      int action_index) {
  if (!runner_)
    return;
  runner_->SimulateWebNotificationClick(title, action_index);
}

void TestRunnerBindings::SimulateWebNotificationClose(const std::string& title,
                                                      bool by_user) {
  if (!runner_)
    return;
  runner_->SimulateWebNotificationClose(title, by_user);
}

void TestRunnerBindings::AddMockSpeechRecognitionResult(
    const std::string& transcript, double confidence) {
  if (runner_)
    runner_->AddMockSpeechRecognitionResult(transcript, confidence);
}

void TestRunnerBindings::SetMockSpeechRecognitionError(
    const std::string& error, const std::string& message) {
  if (runner_)
    runner_->SetMockSpeechRecognitionError(error, message);
}

void TestRunnerBindings::AddMockCredentialManagerResponse(
    const std::string& id,
    const std::string& name,
    const std::string& avatar,
    const std::string& password) {
  if (runner_)
    runner_->AddMockCredentialManagerResponse(id, name, avatar, password);
}

void TestRunnerBindings::AddMockCredentialManagerError(
    const std::string& error) {
  if (runner_)
    runner_->AddMockCredentialManagerError(error);
}

void TestRunnerBindings::AddWebPageOverlay() {
  if (view_runner_)
    view_runner_->AddWebPageOverlay();
}

void TestRunnerBindings::RemoveWebPageOverlay() {
  if (view_runner_)
    view_runner_->RemoveWebPageOverlay();
}

void TestRunnerBindings::LayoutAndPaintAsync() {
  if (view_runner_)
    view_runner_->LayoutAndPaintAsync();
}

void TestRunnerBindings::LayoutAndPaintAsyncThen(
    v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->LayoutAndPaintAsyncThen(callback);
}

void TestRunnerBindings::GetManifestThen(v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->GetManifestThen(callback);
}

void TestRunnerBindings::CapturePixelsAsyncThen(
    v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->CapturePixelsAsyncThen(callback);
}

void TestRunnerBindings::CopyImageAtAndCapturePixelsAsyncThen(
    int x, int y, v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->CopyImageAtAndCapturePixelsAsyncThen(x, y, callback);
}

void TestRunnerBindings::SetCustomTextOutput(const std::string& output) {
  if (runner_)
    runner_->setCustomTextOutput(output);
}

void TestRunnerBindings::SetViewSourceForFrame(const std::string& name,
                                               bool enabled) {
  if (view_runner_)
    view_runner_->SetViewSourceForFrame(name, enabled);
}

void TestRunnerBindings::SetGeofencingMockProvider(bool service_available) {
  if (runner_)
    runner_->SetGeofencingMockProvider(service_available);
}

void TestRunnerBindings::ClearGeofencingMockProvider() {
  if (runner_)
    runner_->ClearGeofencingMockProvider();
}

void TestRunnerBindings::SetGeofencingMockPosition(double latitude,
                                                   double longitude) {
  if (runner_)
    runner_->SetGeofencingMockPosition(latitude, longitude);
}

void TestRunnerBindings::SetPermission(const std::string& name,
                                       const std::string& value,
                                       const std::string& origin,
                                       const std::string& embedding_origin) {
  if (!runner_)
    return;

  return runner_->SetPermission(
      name, value, GURL(origin), GURL(embedding_origin));
}

void TestRunnerBindings::DispatchBeforeInstallPromptEvent(
    int request_id,
    const std::vector<std::string>& event_platforms,
    v8::Local<v8::Function> callback) {
  if (!view_runner_)
    return;

  return view_runner_->DispatchBeforeInstallPromptEvent(
      request_id, event_platforms, callback);
}

void TestRunnerBindings::ResolveBeforeInstallPromptPromise(
    int request_id,
    const std::string& platform) {
  if (!runner_)
    return;

  runner_->ResolveBeforeInstallPromptPromise(request_id, platform);
}

void TestRunnerBindings::RunIdleTasks(v8::Local<v8::Function> callback) {
  if (!view_runner_)
    return;
  view_runner_->RunIdleTasks(callback);
}

std::string TestRunnerBindings::PlatformName() {
  if (runner_)
    return runner_->platform_name_;
  return std::string();
}

std::string TestRunnerBindings::TooltipText() {
  if (runner_)
    return runner_->tooltip_text_;
  return std::string();
}

int TestRunnerBindings::WebHistoryItemCount() {
  if (runner_)
    return runner_->web_history_item_count_;
  return false;
}

bool TestRunnerBindings::InterceptPostMessage() {
  if (runner_)
    return runner_->shouldInterceptPostMessage();
  return false;
}

void TestRunnerBindings::SetInterceptPostMessage(bool value) {
  if (runner_) {
    runner_->layout_test_runtime_flags_.set_intercept_post_message(value);
    runner_->OnLayoutTestRuntimeFlagsChanged();
  }
}

void TestRunnerBindings::ForceNextWebGLContextCreationToFail() {
  if (view_runner_)
    view_runner_->ForceNextWebGLContextCreationToFail();
}

void TestRunnerBindings::ForceNextDrawingBufferCreationToFail() {
  if (view_runner_)
    view_runner_->ForceNextDrawingBufferCreationToFail();
}

void TestRunnerBindings::NotImplemented(const gin::Arguments& args) {
}

TestRunner::WorkQueue::WorkQueue(TestRunner* controller)
    : frozen_(false), controller_(controller), weak_factory_(this) {}

TestRunner::WorkQueue::~WorkQueue() {
  Reset();
}

void TestRunner::WorkQueue::ProcessWorkSoon() {
  if (controller_->topLoadingFrame())
    return;

  if (!queue_.empty()) {
    // We delay processing queued work to avoid recursion problems.
    controller_->delegate_->PostTask(new WebCallbackTask(base::Bind(
        &TestRunner::WorkQueue::ProcessWork, weak_factory_.GetWeakPtr())));
  } else if (!controller_->layout_test_runtime_flags_.wait_until_done()) {
    controller_->delegate_->TestFinished();
  }
}

void TestRunner::WorkQueue::Reset() {
  frozen_ = false;
  while (!queue_.empty()) {
    delete queue_.front();
    queue_.pop_front();
  }
}

void TestRunner::WorkQueue::AddWork(WorkItem* work) {
  if (frozen_) {
    delete work;
    return;
  }
  queue_.push_back(work);
}

void TestRunner::WorkQueue::ProcessWork() {
  // Quit doing work once a load is in progress.
  if (controller_->main_view_) {
    while (!queue_.empty()) {
      bool startedLoad =
          queue_.front()->Run(controller_->delegate_, controller_->main_view_);
      delete queue_.front();
      queue_.pop_front();
      if (startedLoad)
        return;
    }
  }

  if (!controller_->layout_test_runtime_flags_.wait_until_done() &&
      !controller_->topLoadingFrame())
    controller_->delegate_->TestFinished();
}

TestRunner::TestRunner(TestInterfaces* interfaces)
    : test_is_running_(false),
      close_remaining_windows_(false),
      work_queue_(this),
      web_history_item_count_(0),
      test_interfaces_(interfaces),
      delegate_(nullptr),
      main_view_(nullptr),
      mock_content_settings_client_(
          new MockContentSettingsClient(&layout_test_runtime_flags_)),
      credential_manager_client_(new MockCredentialManagerClient),
      mock_screen_orientation_client_(new MockScreenOrientationClient),
      spellcheck_(new SpellCheckClient(this)),
      chooser_count_(0),
      previously_focused_view_(nullptr),
      weak_factory_(this) {}

TestRunner::~TestRunner() {}

void TestRunner::Install(
    WebLocalFrame* frame,
    base::WeakPtr<TestRunnerForSpecificView> view_test_runner) {
  TestRunnerBindings::Install(weak_factory_.GetWeakPtr(), view_test_runner,
                              frame);
}

void TestRunner::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
  mock_content_settings_client_->SetDelegate(delegate);
  spellcheck_->SetDelegate(delegate);
  if (speech_recognizer_)
    speech_recognizer_->SetDelegate(delegate);
}

void TestRunner::SetMainView(WebView* web_view) {
  main_view_ = web_view;
}

void TestRunner::Reset() {
  top_loading_frame_ = nullptr;
  layout_test_runtime_flags_.Reset();
  mock_screen_orientation_client_->ResetData();
  drag_image_.reset();
  widgets_with_scheduled_animations_.clear();

  WebSecurityPolicy::resetOriginAccessWhitelists();
#if defined(__linux__) || defined(ANDROID)
  WebFontRendering::setSubpixelPositioning(false);
#endif

  if (delegate_) {
    // Reset the default quota for each origin to 5MB
    delegate_->SetDatabaseQuota(5 * 1024 * 1024);
    delegate_->SetDeviceColorProfile("reset");
    delegate_->SetDeviceScaleFactor(GetDefaultDeviceScaleFactor());
    delegate_->SetAcceptAllCookies(false);
    delegate_->SetLocale("");
    delegate_->UseUnfortunateSynchronousResizeMode(false);
    delegate_->DisableAutoResizeMode(WebSize());
    delegate_->DeleteAllCookies();
    delegate_->SetBluetoothManualChooser(false);
    delegate_->ClearGeofencingMockProvider();
    delegate_->ResetPermissions();
    ResetDeviceLight();
  }

  dump_as_audio_ = false;
  dump_create_view_ = false;
  can_open_windows_ = false;
  dump_window_status_changes_ = false;
  dump_spell_check_callbacks_ = false;
  dump_back_forward_list_ = false;
  test_repaint_ = false;
  sweep_horizontally_ = false;
  midi_accessor_result_ = true;
  has_custom_text_output_ = false;
  custom_text_output_.clear();

  http_headers_to_clear_.clear();

  platform_name_ = "chromium";
  tooltip_text_ = std::string();
  web_history_item_count_ = 0;

  SetUseMockTheme(true);

  weak_factory_.InvalidateWeakPtrs();
  work_queue_.Reset();

  if (close_remaining_windows_ && delegate_)
    delegate_->CloseRemainingWindows();
  else
    close_remaining_windows_ = true;
}

void TestRunner::SetTestIsRunning(bool running) {
  test_is_running_ = running;
}

bool TestRunner::shouldDumpEditingCallbacks() const {
  return layout_test_runtime_flags_.dump_editting_callbacks();
}

void TestRunner::setShouldDumpAsText(bool value) {
  layout_test_runtime_flags_.set_dump_as_text(value);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::setShouldDumpAsMarkup(bool value) {
  layout_test_runtime_flags_.set_dump_as_markup(value);
  OnLayoutTestRuntimeFlagsChanged();
}

bool TestRunner::shouldDumpAsCustomText() const {
  return has_custom_text_output_;
}

std::string TestRunner::customDumpText() const {
  return custom_text_output_;
}

void TestRunner::setCustomTextOutput(const std::string& text) {
  custom_text_output_ = text;
  has_custom_text_output_ = true;
}

bool TestRunner::ShouldGeneratePixelResults() {
  CheckResponseMimeType();
  return layout_test_runtime_flags_.generate_pixel_results();
}

bool TestRunner::shouldStayOnPageAfterHandlingBeforeUnload() const {
  return layout_test_runtime_flags_.stay_on_page_after_handling_before_unload();
}


void TestRunner::setShouldGeneratePixelResults(bool value) {
  layout_test_runtime_flags_.set_generate_pixel_results(value);
  OnLayoutTestRuntimeFlagsChanged();
}

bool TestRunner::ShouldDumpAsAudio() const {
  return dump_as_audio_;
}

void TestRunner::GetAudioData(std::vector<unsigned char>* buffer_view) const {
  *buffer_view = audio_data_;
}

bool TestRunner::IsRecursiveLayoutDumpRequested() {
  CheckResponseMimeType();
  return layout_test_runtime_flags_.dump_child_frames();
}

std::string TestRunner::DumpLayout(blink::WebLocalFrame* frame) {
  CheckResponseMimeType();
  return ::test_runner::DumpLayout(frame, layout_test_runtime_flags_);
}

void TestRunner::DumpPixelsAsync(
    blink::WebView* web_view,
    const base::Callback<void(const SkBitmap&)>& callback) {
  if (layout_test_runtime_flags_.dump_drag_image()) {
    if (drag_image_.isNull()) {
      // This means the test called dumpDragImage but did not initiate a drag.
      // Return a blank image so that the test fails.
      SkBitmap bitmap;
      bitmap.allocN32Pixels(1, 1);
      {
        SkAutoLockPixels lock(bitmap);
        bitmap.eraseColor(0);
      }
      callback.Run(bitmap);
      return;
    }

    callback.Run(drag_image_.getSkBitmap());
    return;
  }

  test_runner::DumpPixelsAsync(web_view, layout_test_runtime_flags_,
                               delegate_->GetDeviceScaleFactorForTest(),
                               callback);
}

void TestRunner::ReplicateLayoutTestRuntimeFlagsChanges(
    const base::DictionaryValue& changed_values) {
  if (test_is_running_)
    layout_test_runtime_flags_.tracked_dictionary().ApplyUntrackedChanges(
        changed_values);
}

bool TestRunner::HasCustomTextDump(std::string* custom_text_dump) const {
  if (shouldDumpAsCustomText()) {
    *custom_text_dump = customDumpText();
    return true;
  }

  return false;
}

bool TestRunner::shouldDumpFrameLoadCallbacks() const {
  return test_is_running_ &&
         layout_test_runtime_flags_.dump_frame_load_callbacks();
}

void TestRunner::setShouldDumpFrameLoadCallbacks(bool value) {
  layout_test_runtime_flags_.set_dump_frame_load_callbacks(value);
  OnLayoutTestRuntimeFlagsChanged();
}

bool TestRunner::shouldDumpPingLoaderCallbacks() const {
  return test_is_running_ &&
         layout_test_runtime_flags_.dump_ping_loader_callbacks();
}

void TestRunner::setShouldEnableViewSource(bool value) {
  // TODO(lukasza): This flag should be 1) replicated across OOPIFs and
  // 2) applied to all views, not just the main window view.

  // Path-based test config is trigerred by BlinkTestRunner, when |main_view_|
  // is guaranteed to exist at this point.
  DCHECK(main_view_);

  main_view_->mainFrame()->enableViewSourceMode(value);
}

bool TestRunner::shouldDumpUserGestureInFrameLoadCallbacks() const {
  return test_is_running_ &&
         layout_test_runtime_flags_.dump_user_gesture_in_frame_load_callbacks();
}

bool TestRunner::shouldDumpTitleChanges() const {
  return layout_test_runtime_flags_.dump_title_changes();
}

bool TestRunner::shouldDumpIconChanges() const {
  return layout_test_runtime_flags_.dump_icon_changes();
}

bool TestRunner::shouldDumpCreateView() const {
  return dump_create_view_;
}

bool TestRunner::canOpenWindows() const {
  return can_open_windows_;
}

bool TestRunner::shouldDumpResourceLoadCallbacks() const {
  return test_is_running_ &&
         layout_test_runtime_flags_.dump_resource_load_callbacks();
}

bool TestRunner::shouldDumpResourceResponseMIMETypes() const {
  return test_is_running_ &&
         layout_test_runtime_flags_.dump_resource_response_mime_types();
}

WebContentSettingsClient* TestRunner::GetWebContentSettings() const {
  return mock_content_settings_client_.get();
}

void TestRunner::InitializeWebViewWithMocks(blink::WebView* web_view) {
  web_view->setSpellCheckClient(spellcheck_.get());
  web_view->setCredentialManagerClient(credential_manager_client_.get());
}

bool TestRunner::shouldDumpStatusCallbacks() const {
  return dump_window_status_changes_;
}

bool TestRunner::shouldDumpSpellCheckCallbacks() const {
  return dump_spell_check_callbacks_;
}

bool TestRunner::ShouldDumpBackForwardList() const {
  return dump_back_forward_list_;
}

bool TestRunner::isPrinting() const {
  return layout_test_runtime_flags_.is_printing();
}

bool TestRunner::shouldWaitUntilExternalURLLoad() const {
  return layout_test_runtime_flags_.wait_until_external_url_load();
}

const std::set<std::string>* TestRunner::httpHeadersToClear() const {
  return &http_headers_to_clear_;
}

bool TestRunner::IsFramePartOfMainTestWindow(blink::WebFrame* frame) const {
  return test_is_running_ && frame->top()->view() == main_view_;
}

bool TestRunner::tryToSetTopLoadingFrame(WebFrame* frame) {
  if (!IsFramePartOfMainTestWindow(frame))
    return false;

  if (top_loading_frame_ || layout_test_runtime_flags_.have_top_loading_frame())
    return false;

  top_loading_frame_ = frame;
  layout_test_runtime_flags_.set_have_top_loading_frame(true);
  OnLayoutTestRuntimeFlagsChanged();
  return true;
}

bool TestRunner::tryToClearTopLoadingFrame(WebFrame* frame) {
  if (!IsFramePartOfMainTestWindow(frame))
    return false;

  if (frame != top_loading_frame_)
    return false;

  top_loading_frame_ = nullptr;
  DCHECK(layout_test_runtime_flags_.have_top_loading_frame());
  layout_test_runtime_flags_.set_have_top_loading_frame(false);
  OnLayoutTestRuntimeFlagsChanged();

  LocationChangeDone();
  return true;
}

WebFrame* TestRunner::topLoadingFrame() const {
  return top_loading_frame_;
}

void TestRunner::policyDelegateDone() {
  DCHECK(layout_test_runtime_flags_.wait_until_done());
  delegate_->TestFinished();
  layout_test_runtime_flags_.set_wait_until_done(false);
  OnLayoutTestRuntimeFlagsChanged();
}

bool TestRunner::policyDelegateEnabled() const {
  return layout_test_runtime_flags_.policy_delegate_enabled();
}

bool TestRunner::policyDelegateIsPermissive() const {
  return layout_test_runtime_flags_.policy_delegate_is_permissive();
}

bool TestRunner::policyDelegateShouldNotifyDone() const {
  return layout_test_runtime_flags_.policy_delegate_should_notify_done();
}

bool TestRunner::shouldInterceptPostMessage() const {
  return layout_test_runtime_flags_.intercept_post_message();
}

bool TestRunner::shouldDumpResourcePriorities() const {
  return layout_test_runtime_flags_.dump_resource_priorities();
}

void TestRunner::setToolTipText(const WebString& text) {
  tooltip_text_ = text.utf8();
}

void TestRunner::setDragImage(
    const blink::WebImage& drag_image) {
  if (layout_test_runtime_flags_.dump_drag_image()) {
    if (drag_image_.isNull())
      drag_image_ = drag_image;
  }
}

bool TestRunner::shouldDumpNavigationPolicy() const {
  return layout_test_runtime_flags_.dump_navigation_policy();
}

bool TestRunner::midiAccessorResult() {
  return midi_accessor_result_;
}

void TestRunner::ClearDevToolsLocalStorage() {
  delegate_->ClearDevToolsLocalStorage();
}

void TestRunner::ShowDevTools(const std::string& settings,
                              const std::string& frontend_url) {
  delegate_->ShowDevTools(settings, frontend_url);
}

class WorkItemBackForward : public TestRunner::WorkItem {
 public:
  WorkItemBackForward(int distance) : distance_(distance) {}

  bool Run(WebTestDelegate* delegate, WebView*) override {
    delegate->GoToOffset(distance_);
    return true; // FIXME: Did it really start a navigation?
  }

 private:
  int distance_;
};

void TestRunner::NotifyDone() {
  // Test didn't timeout. Kill the pending callbacks.
  weak_factory_.InvalidateWeakPtrs();

  CompleteNotifyDone();
}

void TestRunner::WaitUntilDone() {
  layout_test_runtime_flags_.set_wait_until_done(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::QueueBackNavigation(int how_far_back) {
  work_queue_.AddWork(new WorkItemBackForward(-how_far_back));
}

void TestRunner::QueueForwardNavigation(int how_far_forward) {
  work_queue_.AddWork(new WorkItemBackForward(how_far_forward));
}

class WorkItemReload : public TestRunner::WorkItem {
 public:
  bool Run(WebTestDelegate* delegate, WebView*) override {
    delegate->Reload();
    return true;
  }
};

void TestRunner::QueueReload() {
  work_queue_.AddWork(new WorkItemReload());
}

class WorkItemLoadingScript : public TestRunner::WorkItem {
 public:
  WorkItemLoadingScript(const std::string& script)
      : script_(script) {}

  bool Run(WebTestDelegate*, WebView* web_view) override {
    web_view->mainFrame()->executeScript(
        WebScriptSource(WebString::fromUTF8(script_)));
    return true; // FIXME: Did it really start a navigation?
  }

 private:
  std::string script_;
};

void TestRunner::QueueLoadingScript(const std::string& script) {
  work_queue_.AddWork(new WorkItemLoadingScript(script));
}

class WorkItemNonLoadingScript : public TestRunner::WorkItem {
 public:
  WorkItemNonLoadingScript(const std::string& script)
      : script_(script) {}

  bool Run(WebTestDelegate*, WebView* web_view) override {
    web_view->mainFrame()->executeScript(
        WebScriptSource(WebString::fromUTF8(script_)));
    return false;
  }

 private:
  std::string script_;
};

void TestRunner::QueueNonLoadingScript(const std::string& script) {
  work_queue_.AddWork(new WorkItemNonLoadingScript(script));
}

class WorkItemLoad : public TestRunner::WorkItem {
 public:
  WorkItemLoad(const WebURL& url, const std::string& target)
      : url_(url), target_(target) {}

  bool Run(WebTestDelegate* delegate, WebView*) override {
    delegate->LoadURLForFrame(url_, target_);
    return true; // FIXME: Did it really start a navigation?
  }

 private:
  WebURL url_;
  std::string target_;
};

void TestRunner::QueueLoad(const std::string& url, const std::string& target) {
  if (!main_view_)
    return;

  // FIXME: Implement WebURL::resolve() and avoid GURL.
  GURL current_url = main_view_->mainFrame()->document().url();
  GURL full_url = current_url.Resolve(url);
  work_queue_.AddWork(new WorkItemLoad(full_url, target));
}

void TestRunner::SetCustomPolicyDelegate(gin::Arguments* args) {
  bool value;
  args->GetNext(&value);
  layout_test_runtime_flags_.set_policy_delegate_enabled(value);

  if (!args->PeekNext().IsEmpty() && args->PeekNext()->IsBoolean()) {
    args->GetNext(&value);
    layout_test_runtime_flags_.set_policy_delegate_is_permissive(value);
  }

  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::WaitForPolicyDelegate() {
  layout_test_runtime_flags_.set_policy_delegate_enabled(true);
  layout_test_runtime_flags_.set_policy_delegate_should_notify_done(true);
  layout_test_runtime_flags_.set_wait_until_done(true);
  OnLayoutTestRuntimeFlagsChanged();
}

int TestRunner::WindowCount() {
  return test_interfaces_->GetWindowList().size();
}

void TestRunner::SetCloseRemainingWindowsWhenComplete(
    bool close_remaining_windows) {
  close_remaining_windows_ = close_remaining_windows;
}

void TestRunner::ResetTestHelperControllers() {
  test_interfaces_->ResetTestHelperControllers();
}

void TestRunner::AddOriginAccessWhitelistEntry(
    const std::string& source_origin,
    const std::string& destination_protocol,
    const std::string& destination_host,
    bool allow_destination_subdomains) {
  WebURL url((GURL(source_origin)));
  if (!url.isValid())
    return;

  WebSecurityPolicy::addOriginAccessWhitelistEntry(
      url,
      WebString::fromUTF8(destination_protocol),
      WebString::fromUTF8(destination_host),
      allow_destination_subdomains);
}

void TestRunner::RemoveOriginAccessWhitelistEntry(
    const std::string& source_origin,
    const std::string& destination_protocol,
    const std::string& destination_host,
    bool allow_destination_subdomains) {
  WebURL url((GURL(source_origin)));
  if (!url.isValid())
    return;

  WebSecurityPolicy::removeOriginAccessWhitelistEntry(
      url,
      WebString::fromUTF8(destination_protocol),
      WebString::fromUTF8(destination_host),
      allow_destination_subdomains);
}

void TestRunner::SetTextSubpixelPositioning(bool value) {
#if defined(__linux__) || defined(ANDROID)
  // Since FontConfig doesn't provide a variable to control subpixel
  // positioning, we'll fall back to setting it globally for all fonts.
  WebFontRendering::setSubpixelPositioning(value);
#endif
}

void TestRunner::UseUnfortunateSynchronousResizeMode() {
  delegate_->UseUnfortunateSynchronousResizeMode(true);
}

bool TestRunner::EnableAutoResizeMode(int min_width,
                                      int min_height,
                                      int max_width,
                                      int max_height) {
  WebSize min_size(min_width, min_height);
  WebSize max_size(max_width, max_height);
  delegate_->EnableAutoResizeMode(min_size, max_size);
  return true;
}

bool TestRunner::DisableAutoResizeMode(int new_width, int new_height) {
  WebSize new_size(new_width, new_height);
  delegate_->DisableAutoResizeMode(new_size);
  return true;
}

void TestRunner::SetMockDeviceLight(double value) {
  delegate_->SetDeviceLightData(value);
}

void TestRunner::ResetDeviceLight() {
  delegate_->SetDeviceLightData(-1);
}

void TestRunner::SetMockDeviceMotion(
    bool has_acceleration_x, double acceleration_x,
    bool has_acceleration_y, double acceleration_y,
    bool has_acceleration_z, double acceleration_z,
    bool has_acceleration_including_gravity_x,
    double acceleration_including_gravity_x,
    bool has_acceleration_including_gravity_y,
    double acceleration_including_gravity_y,
    bool has_acceleration_including_gravity_z,
    double acceleration_including_gravity_z,
    bool has_rotation_rate_alpha, double rotation_rate_alpha,
    bool has_rotation_rate_beta, double rotation_rate_beta,
    bool has_rotation_rate_gamma, double rotation_rate_gamma,
    double interval) {
  WebDeviceMotionData motion;

  // acceleration
  motion.hasAccelerationX = has_acceleration_x;
  motion.accelerationX = acceleration_x;
  motion.hasAccelerationY = has_acceleration_y;
  motion.accelerationY = acceleration_y;
  motion.hasAccelerationZ = has_acceleration_z;
  motion.accelerationZ = acceleration_z;

  // accelerationIncludingGravity
  motion.hasAccelerationIncludingGravityX =
      has_acceleration_including_gravity_x;
  motion.accelerationIncludingGravityX = acceleration_including_gravity_x;
  motion.hasAccelerationIncludingGravityY =
      has_acceleration_including_gravity_y;
  motion.accelerationIncludingGravityY = acceleration_including_gravity_y;
  motion.hasAccelerationIncludingGravityZ =
      has_acceleration_including_gravity_z;
  motion.accelerationIncludingGravityZ = acceleration_including_gravity_z;

  // rotationRate
  motion.hasRotationRateAlpha = has_rotation_rate_alpha;
  motion.rotationRateAlpha = rotation_rate_alpha;
  motion.hasRotationRateBeta = has_rotation_rate_beta;
  motion.rotationRateBeta = rotation_rate_beta;
  motion.hasRotationRateGamma = has_rotation_rate_gamma;
  motion.rotationRateGamma = rotation_rate_gamma;

  // interval
  motion.interval = interval;

  delegate_->SetDeviceMotionData(motion);
}

void TestRunner::SetMockDeviceOrientation(bool has_alpha, double alpha,
                                          bool has_beta, double beta,
                                          bool has_gamma, double gamma,
                                          bool absolute) {
  WebDeviceOrientationData orientation;

  // alpha
  orientation.hasAlpha = has_alpha;
  orientation.alpha = alpha;

  // beta
  orientation.hasBeta = has_beta;
  orientation.beta = beta;

  // gamma
  orientation.hasGamma = has_gamma;
  orientation.gamma = gamma;

  // absolute
  orientation.absolute = absolute;

  delegate_->SetDeviceOrientationData(orientation);
}

MockScreenOrientationClient* TestRunner::getMockScreenOrientationClient() {
  return mock_screen_orientation_client_.get();
}

MockWebUserMediaClient* TestRunner::getMockWebUserMediaClient() {
  if (!user_media_client_.get())
    user_media_client_.reset(new MockWebUserMediaClient(delegate_));
  return user_media_client_.get();
}

MockWebSpeechRecognizer* TestRunner::getMockWebSpeechRecognizer() {
  if (!speech_recognizer_.get()) {
    speech_recognizer_.reset(new MockWebSpeechRecognizer());
    speech_recognizer_->SetDelegate(delegate_);
  }
  return speech_recognizer_.get();
}

void TestRunner::SetMockScreenOrientation(const std::string& orientation_str) {
  blink::WebScreenOrientationType orientation;

  if (orientation_str == "portrait-primary") {
    orientation = WebScreenOrientationPortraitPrimary;
  } else if (orientation_str == "portrait-secondary") {
    orientation = WebScreenOrientationPortraitSecondary;
  } else if (orientation_str == "landscape-primary") {
    orientation = WebScreenOrientationLandscapePrimary;
  } else {
    DCHECK_EQ("landscape-secondary", orientation_str);
    orientation = WebScreenOrientationLandscapeSecondary;
  }

  for (WebTestProxyBase* window : test_interfaces_->GetWindowList()) {
    WebFrame* main_frame = window->web_view()->mainFrame();
    // TODO(lukasza): Need to make this work for remote frames.
    if (main_frame->isWebLocalFrame()) {
      mock_screen_orientation_client_->UpdateDeviceOrientation(
          main_frame->toWebLocalFrame(), orientation);
    }
  }
}

void TestRunner::DisableMockScreenOrientation() {
  mock_screen_orientation_client_->SetDisabled(true);
}

void TestRunner::DidOpenChooser() {
  chooser_count_++;
}

void TestRunner::DidCloseChooser() {
  chooser_count_--;
  DCHECK_LE(0, chooser_count_);
}

void TestRunner::SetPopupBlockingEnabled(bool block_popups) {
  delegate_->Preferences()->java_script_can_open_windows_automatically =
      !block_popups;
  delegate_->ApplyPreferences();
}

void TestRunner::SetJavaScriptCanAccessClipboard(bool can_access) {
  delegate_->Preferences()->java_script_can_access_clipboard = can_access;
  delegate_->ApplyPreferences();
}

void TestRunner::SetXSSAuditorEnabled(bool enabled) {
  delegate_->Preferences()->xss_auditor_enabled = enabled;
  delegate_->ApplyPreferences();
}

void TestRunner::SetAllowUniversalAccessFromFileURLs(bool allow) {
  delegate_->Preferences()->allow_universal_access_from_file_urls = allow;
  delegate_->ApplyPreferences();
}

void TestRunner::SetAllowFileAccessFromFileURLs(bool allow) {
  delegate_->Preferences()->allow_file_access_from_file_urls = allow;
  delegate_->ApplyPreferences();
}

void TestRunner::OverridePreference(const std::string& key,
                                    v8::Local<v8::Value> value) {
  TestPreferences* prefs = delegate_->Preferences();
  if (key == "WebKitDefaultFontSize") {
    prefs->default_font_size = value->Int32Value();
  } else if (key == "WebKitMinimumFontSize") {
    prefs->minimum_font_size = value->Int32Value();
  } else if (key == "WebKitDefaultTextEncodingName") {
    v8::Isolate* isolate = blink::mainThreadIsolate();
    prefs->default_text_encoding_name =
        V8StringToWebString(value->ToString(isolate));
  } else if (key == "WebKitJavaScriptEnabled") {
    prefs->java_script_enabled = value->BooleanValue();
  } else if (key == "WebKitSupportsMultipleWindows") {
    prefs->supports_multiple_windows = value->BooleanValue();
  } else if (key == "WebKitDisplayImagesKey") {
    prefs->loads_images_automatically = value->BooleanValue();
  } else if (key == "WebKitPluginsEnabled") {
    prefs->plugins_enabled = value->BooleanValue();
  } else if (key == "WebKitTabToLinksPreferenceKey") {
    prefs->tabs_to_links = value->BooleanValue();
  } else if (key == "WebKitWebGLEnabled") {
    prefs->experimental_webgl_enabled = value->BooleanValue();
  } else if (key == "WebKitCSSGridLayoutEnabled") {
    prefs->experimental_css_grid_layout_enabled = value->BooleanValue();
  } else if (key == "WebKitHyperlinkAuditingEnabled") {
    prefs->hyperlink_auditing_enabled = value->BooleanValue();
  } else if (key == "WebKitEnableCaretBrowsing") {
    prefs->caret_browsing_enabled = value->BooleanValue();
  } else if (key == "WebKitAllowDisplayingInsecureContent") {
    prefs->allow_display_of_insecure_content = value->BooleanValue();
  } else if (key == "WebKitAllowRunningInsecureContent") {
    prefs->allow_running_of_insecure_content = value->BooleanValue();
  } else if (key == "WebKitDisableReadingFromCanvas") {
    prefs->disable_reading_from_canvas = value->BooleanValue();
  } else if (key == "WebKitStrictMixedContentChecking") {
    prefs->strict_mixed_content_checking = value->BooleanValue();
  } else if (key == "WebKitStrictPowerfulFeatureRestrictions") {
    prefs->strict_powerful_feature_restrictions = value->BooleanValue();
  } else if (key == "WebKitShouldRespectImageOrientation") {
    prefs->should_respect_image_orientation = value->BooleanValue();
  } else if (key == "WebKitWebSecurityEnabled") {
    prefs->web_security_enabled = value->BooleanValue();
  } else {
    std::string message("Invalid name for preference: ");
    message.append(key);
    delegate_->PrintMessage(std::string("CONSOLE MESSAGE: ") + message + "\n");
  }
  delegate_->ApplyPreferences();
}

std::string TestRunner::GetAcceptLanguages() const {
  return layout_test_runtime_flags_.accept_languages();
}

void TestRunner::SetAcceptLanguages(const std::string& accept_languages) {
  if (accept_languages == GetAcceptLanguages())
    return;

  layout_test_runtime_flags_.set_accept_languages(accept_languages);
  OnLayoutTestRuntimeFlagsChanged();

  for (WebTestProxyBase* window : test_interfaces_->GetWindowList())
    window->web_view()->acceptLanguagesChanged();
}

void TestRunner::SetPluginsEnabled(bool enabled) {
  delegate_->Preferences()->plugins_enabled = enabled;
  delegate_->ApplyPreferences();
}

bool TestRunner::GetAnimationScheduled() const {
  bool is_animation_scheduled = !widgets_with_scheduled_animations_.empty();
  return is_animation_scheduled;
}

void TestRunner::OnAnimationScheduled(blink::WebWidget* widget) {
  widgets_with_scheduled_animations_.insert(widget);
}

void TestRunner::OnAnimationBegun(blink::WebWidget* widget) {
  widgets_with_scheduled_animations_.erase(widget);
}

void TestRunner::DumpEditingCallbacks() {
  layout_test_runtime_flags_.set_dump_editting_callbacks(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpAsMarkup() {
  layout_test_runtime_flags_.set_dump_as_markup(true);
  layout_test_runtime_flags_.set_generate_pixel_results(false);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpAsText() {
  layout_test_runtime_flags_.set_dump_as_text(true);
  layout_test_runtime_flags_.set_generate_pixel_results(false);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpAsTextWithPixelResults() {
  layout_test_runtime_flags_.set_dump_as_text(true);
  layout_test_runtime_flags_.set_generate_pixel_results(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpChildFrameScrollPositions() {
  layout_test_runtime_flags_.set_dump_child_frame_scroll_positions(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpChildFramesAsMarkup() {
  layout_test_runtime_flags_.set_dump_child_frames_as_markup(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpChildFramesAsText() {
  layout_test_runtime_flags_.set_dump_child_frames_as_text(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpIconChanges() {
  layout_test_runtime_flags_.set_dump_icon_changes(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetAudioData(const gin::ArrayBufferView& view) {
  unsigned char* bytes = static_cast<unsigned char*>(view.bytes());
  audio_data_.resize(view.num_bytes());
  std::copy(bytes, bytes + view.num_bytes(), audio_data_.begin());
  dump_as_audio_ = true;
}

void TestRunner::DumpFrameLoadCallbacks() {
  layout_test_runtime_flags_.set_dump_frame_load_callbacks(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpPingLoaderCallbacks() {
  layout_test_runtime_flags_.set_dump_ping_loader_callbacks(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpUserGestureInFrameLoadCallbacks() {
  layout_test_runtime_flags_.set_dump_user_gesture_in_frame_load_callbacks(
      true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpTitleChanges() {
  layout_test_runtime_flags_.set_dump_title_changes(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpCreateView() {
  dump_create_view_ = true;
}

void TestRunner::SetCanOpenWindows() {
  can_open_windows_ = true;
}

void TestRunner::DumpResourceLoadCallbacks() {
  layout_test_runtime_flags_.set_dump_resource_load_callbacks(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpResourceResponseMIMETypes() {
  layout_test_runtime_flags_.set_dump_resource_response_mime_types(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetImagesAllowed(bool allowed) {
  layout_test_runtime_flags_.set_images_allowed(allowed);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetMediaAllowed(bool allowed) {
  layout_test_runtime_flags_.set_media_allowed(allowed);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetScriptsAllowed(bool allowed) {
  layout_test_runtime_flags_.set_scripts_allowed(allowed);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetStorageAllowed(bool allowed) {
  layout_test_runtime_flags_.set_storage_allowed(allowed);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetPluginsAllowed(bool allowed) {
  layout_test_runtime_flags_.set_plugins_allowed(allowed);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetAllowDisplayOfInsecureContent(bool allowed) {
  layout_test_runtime_flags_.set_displaying_insecure_content_allowed(allowed);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetAllowRunningOfInsecureContent(bool allowed) {
  layout_test_runtime_flags_.set_running_insecure_content_allowed(allowed);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetAutoplayAllowed(bool allowed) {
  layout_test_runtime_flags_.set_autoplay_allowed(allowed);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpPermissionClientCallbacks() {
  layout_test_runtime_flags_.set_dump_web_content_settings_client_callbacks(
      true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpWindowStatusChanges() {
  dump_window_status_changes_ = true;
}

void TestRunner::DumpSpellCheckCallbacks() {
  dump_spell_check_callbacks_ = true;
}

void TestRunner::DumpBackForwardList() {
  dump_back_forward_list_ = true;
}

void TestRunner::DumpSelectionRect() {
  layout_test_runtime_flags_.set_dump_selection_rect(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetPrinting() {
  layout_test_runtime_flags_.set_is_printing(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::ClearPrinting() {
  layout_test_runtime_flags_.set_is_printing(false);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetShouldStayOnPageAfterHandlingBeforeUnload(bool value) {
  layout_test_runtime_flags_.set_stay_on_page_after_handling_before_unload(
      value);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetWillSendRequestClearHeader(const std::string& header) {
  if (!header.empty())
    http_headers_to_clear_.insert(header);
}

void TestRunner::DumpResourceRequestPriorities() {
  layout_test_runtime_flags_.set_dump_resource_priorities(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetUseMockTheme(bool use) {
  use_mock_theme_ = use;
  blink::setMockThemeEnabledForTest(use);
}

void TestRunner::ShowWebInspector(const std::string& str,
                                  const std::string& frontend_url) {
  ShowDevTools(str, frontend_url);
}

void TestRunner::WaitUntilExternalURLLoad() {
  layout_test_runtime_flags_.set_wait_until_external_url_load(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpDragImage() {
  layout_test_runtime_flags_.set_dump_drag_image(true);
  DumpAsTextWithPixelResults();
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::DumpNavigationPolicy() {
  layout_test_runtime_flags_.set_dump_navigation_policy(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::CloseWebInspector() {
  delegate_->CloseDevTools();
}

bool TestRunner::IsChooserShown() {
  return 0 < chooser_count_;
}

void TestRunner::EvaluateInWebInspector(int call_id,
                                        const std::string& script) {
  delegate_->EvaluateInWebInspector(call_id, script);
}

std::string TestRunner::EvaluateInWebInspectorOverlay(
    const std::string& script) {
  return delegate_->EvaluateInWebInspectorOverlay(script);
}

void TestRunner::ClearAllDatabases() {
  delegate_->ClearAllDatabases();
}

void TestRunner::SetDatabaseQuota(int quota) {
  delegate_->SetDatabaseQuota(quota);
}

void TestRunner::SetAlwaysAcceptCookies(bool accept) {
  delegate_->SetAcceptAllCookies(accept);
}

void TestRunner::SetFocus(blink::WebView* web_view, bool focus) {
  if (focus) {
    if (previously_focused_view_ != web_view) {
      delegate_->SetFocus(previously_focused_view_, false);
      delegate_->SetFocus(web_view, true);
      previously_focused_view_ = web_view;
    }
  } else {
    if (previously_focused_view_ == web_view) {
      delegate_->SetFocus(web_view, false);
      previously_focused_view_ = nullptr;
    }
  }
}

std::string TestRunner::PathToLocalResource(const std::string& path) {
  return delegate_->PathToLocalResource(path);
}

void TestRunner::SetGeofencingMockProvider(bool service_available) {
  delegate_->SetGeofencingMockProvider(service_available);
}

void TestRunner::ClearGeofencingMockProvider() {
  delegate_->ClearGeofencingMockProvider();
}

void TestRunner::SetGeofencingMockPosition(double latitude, double longitude) {
  delegate_->SetGeofencingMockPosition(latitude, longitude);
}

void TestRunner::SetPermission(const std::string& name,
                               const std::string& value,
                               const GURL& origin,
                               const GURL& embedding_origin) {
  delegate_->SetPermission(name, value, origin, embedding_origin);
}

void TestRunner::ResolveBeforeInstallPromptPromise(
    int request_id,
    const std::string& platform) {
  if (!test_interfaces_->GetAppBannerClient())
    return;
  test_interfaces_->GetAppBannerClient()->ResolvePromise(request_id, platform);
}

void TestRunner::SetPOSIXLocale(const std::string& locale) {
  delegate_->SetLocale(locale);
}

void TestRunner::SetMIDIAccessorResult(bool result) {
  midi_accessor_result_ = result;
}

void TestRunner::SimulateWebNotificationClick(const std::string& title,
                                              int action_index) {
  delegate_->SimulateWebNotificationClick(title, action_index);
}

void TestRunner::SimulateWebNotificationClose(const std::string& title,
                                              bool by_user) {
  delegate_->SimulateWebNotificationClose(title, by_user);
}

void TestRunner::AddMockSpeechRecognitionResult(const std::string& transcript,
                                                double confidence) {
  getMockWebSpeechRecognizer()->AddMockResult(WebString::fromUTF8(transcript),
                                              confidence);
}

void TestRunner::SetMockSpeechRecognitionError(const std::string& error,
                                               const std::string& message) {
  getMockWebSpeechRecognizer()->SetError(WebString::fromUTF8(error),
                                         WebString::fromUTF8(message));
}

void TestRunner::AddMockCredentialManagerResponse(const std::string& id,
                                                  const std::string& name,
                                                  const std::string& avatar,
                                                  const std::string& password) {
  credential_manager_client_->SetResponse(new WebPasswordCredential(
      WebString::fromUTF8(id), WebString::fromUTF8(password),
      WebString::fromUTF8(name), WebURL(GURL(avatar))));
}

void TestRunner::AddMockCredentialManagerError(const std::string& error) {
  credential_manager_client_->SetError(error);
}

void TestRunner::OnLayoutTestRuntimeFlagsChanged() {
  if (layout_test_runtime_flags_.tracked_dictionary().changed_values().empty())
    return;
  if (!test_is_running_)
    return;

  delegate_->OnLayoutTestRuntimeFlagsChanged(
      layout_test_runtime_flags_.tracked_dictionary().changed_values());
  layout_test_runtime_flags_.tracked_dictionary().ResetChangeTracking();
}

void TestRunner::LocationChangeDone() {
  web_history_item_count_ = delegate_->NavigationEntryCount();

  // No more new work after the first complete load.
  work_queue_.set_frozen(true);

  if (!layout_test_runtime_flags_.wait_until_done())
    work_queue_.ProcessWorkSoon();
}

void TestRunner::CheckResponseMimeType() {
  // Text output: the test page can request different types of output which we
  // handle here.

  if (layout_test_runtime_flags_.dump_as_text())
    return;

  if (!main_view_)
    return;

  WebDataSource* data_source = main_view_->mainFrame()->dataSource();
  if (!data_source)
    return;

  std::string mimeType = data_source->response().mimeType().utf8();
  if (mimeType != "text/plain")
    return;

  layout_test_runtime_flags_.set_dump_as_text(true);
  layout_test_runtime_flags_.set_generate_pixel_results(false);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::CompleteNotifyDone() {
  if (layout_test_runtime_flags_.wait_until_done() && !topLoadingFrame() &&
      work_queue_.is_empty())
    delegate_->TestFinished();
  layout_test_runtime_flags_.set_wait_until_done(false);
  OnLayoutTestRuntimeFlagsChanged();
}

}  // namespace test_runner
