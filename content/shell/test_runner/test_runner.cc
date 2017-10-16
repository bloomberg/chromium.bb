// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/test_runner.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/shell/test_runner/layout_and_paint_async_then.h"
#include "content/shell/test_runner/layout_dump.h"
#include "content/shell/test_runner/mock_content_settings_client.h"
#include "content/shell/test_runner/mock_credential_manager_client.h"
#include "content/shell/test_runner/mock_screen_orientation_client.h"
#include "content/shell/test_runner/mock_web_document_subresource_filter.h"
#include "content/shell/test_runner/mock_web_speech_recognizer.h"
#include "content/shell/test_runner/mock_web_user_media_client.h"
#include "content/shell/test_runner/pixel_dump.h"
#include "content/shell/test_runner/spell_check_client.h"
#include "content/shell/test_runner/test_common.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_preferences.h"
#include "content/shell/test_runner/test_runner_for_specific_view.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "device/sensors/public/cpp/motion_data.h"
#include "device/sensors/public/cpp/orientation_data.h"
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
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "third_party/WebKit/public/web/WebArrayBuffer.h"
#include "third_party/WebKit/public/web/WebArrayBufferConverter.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebDocumentLoader.h"
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
                      WebLocalFrame* frame,
                      bool is_web_platform_tests_mode);

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
  void DumpPermissionClientCallbacks();
  void DumpPingLoaderCallbacks();
  void DumpResourceLoadCallbacks();
  void DumpResourceResponseMIMETypes();
  void DumpSelectionRect();
  void DumpSpellCheckCallbacks();
  void DumpTitleChanges();
  void DumpUserGestureInFrameLoadCallbacks();
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
  void RemoveSpellCheckResolvedCallback();
  void RemoveWebPageOverlay();
  void ResetTestHelperControllers();
  void ResolveBeforeInstallPromptPromise(const std::string& platform);
  void RunIdleTasks(v8::Local<v8::Function> callback);
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument);
  void SetAcceptLanguages(const std::string& accept_languages);
  void SetAllowFileAccessFromFileURLs(bool allow);
  void SetAllowRunningOfInsecureContent(bool allowed);
  void SetAutoplayAllowed(bool allowed);
  void SetAllowUniversalAccessFromFileURLs(bool allow);
  void SetBlockThirdPartyCookies(bool block);
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
  void SetDisallowedSubresourcePathSuffixes(
      const std::vector<std::string>& suffixes);
  void SetDomainRelaxationForbiddenForURLScheme(bool forbidden,
                                                const std::string& scheme);
  void SetDumpConsoleMessages(bool value);
  void SetDumpJavaScriptDialogs(bool value);
  void SetEffectiveConnectionType(const std::string& connection_type);
  void SetMockSpellCheckerEnabled(bool enabled);
  void SetImagesAllowed(bool allowed);
  void SetIsolatedWorldContentSecurityPolicy(int world_id,
                                             const std::string& policy);
  void SetIsolatedWorldSecurityOrigin(int world_id,
                                      v8::Local<v8::Value> origin);
  void SetJavaScriptCanAccessClipboard(bool can_access);
  void SetMIDIAccessorResult(bool result);
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
  void SetShouldGeneratePixelResults(bool);
  void SetShouldStayOnPageAfterHandlingBeforeUnload(bool value);
  void SetSpellCheckResolvedCallback(v8::Local<v8::Function> callback);
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
  void SimulateWebNotificationClick(gin::Arguments* args);
  void SimulateWebNotificationClose(const std::string& title, bool by_user);
  void UseUnfortunateSynchronousResizeMode();
  void WaitForPolicyDelegate();
  void WaitUntilDone();
  void WaitUntilExternalURLLoad();
  void SetMockCredentialManagerError(const std::string& error);
  void SetMockCredentialManagerResponse(const std::string& id,
                                        const std::string& name,
                                        const std::string& avatar,
                                        const std::string& password);
  void ClearMockCredentialManagerResponse();
  bool CallShouldCloseOnWebView();
  bool DisableAutoResizeMode(int new_width, int new_height);
  bool EnableAutoResizeMode(int min_width,
                            int min_height,
                            int max_width,
                            int max_height);
  std::string EvaluateInWebInspectorOverlay(const std::string& script);
  v8::Local<v8::Value> EvaluateScriptInIsolatedWorldAndReturnValue(
      int world_id,
      const std::string& script);
  bool FindString(const std::string& search_text,
                  const std::vector<std::string>& options_array);
  bool HasCustomPageSizeStyle(int page_index);
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

gin::WrapperInfo TestRunnerBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void TestRunnerBindings::Install(
    base::WeakPtr<TestRunner> test_runner,
    base::WeakPtr<TestRunnerForSpecificView> view_test_runner,
    WebLocalFrame* frame,
    bool is_web_platform_tests_mode) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
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

  // The web-platform-tests suite require that reference comparison is delayed
  // for any test with a 'reftest-wait' class on the root element, until that
  // class attribute is removed. To support this approach, we inject some
  // JavaScript that implements the same behavior using TestRunner.
  //
  // See http://web-platform-tests.org/writing-tests/reftests.html for more
  // details about reference tests in the web-platform-tests suite.
  if (is_web_platform_tests_mode) {
    frame->ExecuteScript(blink::WebString(
        R"(window.addEventListener('load', function() {
          if (!window.testRunner) {
            return;
          }
          const target = document.documentElement;
          if (target != null && target.classList.contains('reftest-wait')) {
            window.testRunner.waitUntilDone();
            const observer = new MutationObserver(function(mutations) {
              mutations.forEach(function(mutation) {
                if (!target.classList.contains('reftest-wait')) {
                  window.testRunner.notifyDone();
                }
              });
            });
            const config = {attributes: true};
            observer.observe(target, config);
          }
        });)"));
  }
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
      .SetMethod("setMockCredentialManagerError",
                 &TestRunnerBindings::SetMockCredentialManagerError)
      .SetMethod("setMockCredentialManagerResponse",
                 &TestRunnerBindings::SetMockCredentialManagerResponse)
      .SetMethod("clearMockCredentialManagerResponse",
                 &TestRunnerBindings::ClearMockCredentialManagerResponse)
      .SetMethod("addMockSpeechRecognitionResult",
                 &TestRunnerBindings::AddMockSpeechRecognitionResult)
      .SetMethod("addOriginAccessWhitelistEntry",
                 &TestRunnerBindings::AddOriginAccessWhitelistEntry)
      .SetMethod("addWebPageOverlay", &TestRunnerBindings::AddWebPageOverlay)
      .SetMethod("callShouldCloseOnWebView",
                 &TestRunnerBindings::CallShouldCloseOnWebView)
      .SetMethod("capturePixelsAsyncThen",
                 &TestRunnerBindings::CapturePixelsAsyncThen)
      .SetMethod("clearAllDatabases", &TestRunnerBindings::ClearAllDatabases)
      .SetMethod("clearBackForwardList", &TestRunnerBindings::NotImplemented)
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
      .SetMethod("setDisallowedSubresourcePathSuffixes",
                 &TestRunnerBindings::SetDisallowedSubresourcePathSuffixes)
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
      .SetMethod("dumpPermissionClientCallbacks",
                 &TestRunnerBindings::DumpPermissionClientCallbacks)
      .SetMethod("dumpPingLoaderCallbacks",
                 &TestRunnerBindings::DumpPingLoaderCallbacks)
      .SetMethod("dumpResourceLoadCallbacks",
                 &TestRunnerBindings::DumpResourceLoadCallbacks)
      .SetMethod("dumpResourceResponseMIMETypes",
                 &TestRunnerBindings::DumpResourceResponseMIMETypes)
      .SetMethod("dumpSelectionRect", &TestRunnerBindings::DumpSelectionRect)
      .SetMethod("dumpSpellCheckCallbacks",
                 &TestRunnerBindings::DumpSpellCheckCallbacks)
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
      .SetMethod("removeSpellCheckResolvedCallback",
                 &TestRunnerBindings::RemoveSpellCheckResolvedCallback)
      .SetMethod("removeWebPageOverlay",
                 &TestRunnerBindings::RemoveWebPageOverlay)
      .SetMethod("resetTestHelperControllers",
                 &TestRunnerBindings::ResetTestHelperControllers)
      .SetMethod("resolveBeforeInstallPromptPromise",
                 &TestRunnerBindings::ResolveBeforeInstallPromptPromise)
      .SetMethod("runIdleTasks", &TestRunnerBindings::RunIdleTasks)
      .SetMethod("selectionAsMarkup", &TestRunnerBindings::SelectionAsMarkup)

      // The Bluetooth functions are specified at
      // https://webbluetoothcg.github.io/web-bluetooth/tests/.
      .SetMethod("sendBluetoothManualChooserEvent",
                 &TestRunnerBindings::SendBluetoothManualChooserEvent)
      .SetMethod("setAcceptLanguages", &TestRunnerBindings::SetAcceptLanguages)
      .SetMethod("setAllowFileAccessFromFileURLs",
                 &TestRunnerBindings::SetAllowFileAccessFromFileURLs)
      .SetMethod("setAllowRunningOfInsecureContent",
                 &TestRunnerBindings::SetAllowRunningOfInsecureContent)
      .SetMethod("setAutoplayAllowed", &TestRunnerBindings::SetAutoplayAllowed)
      .SetMethod("setAllowUniversalAccessFromFileURLs",
                 &TestRunnerBindings::SetAllowUniversalAccessFromFileURLs)
      .SetMethod("setBlockThirdPartyCookies",
                 &TestRunnerBindings::SetBlockThirdPartyCookies)
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
      .SetMethod("setDumpConsoleMessages",
                 &TestRunnerBindings::SetDumpConsoleMessages)
      .SetMethod("setDumpJavaScriptDialogs",
                 &TestRunnerBindings::SetDumpJavaScriptDialogs)
      .SetMethod("setEffectiveConnectionType",
                 &TestRunnerBindings::SetEffectiveConnectionType)
      .SetMethod("setMockSpellCheckerEnabled",
                 &TestRunnerBindings::SetMockSpellCheckerEnabled)
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
      .SetMethod("setShouldGeneratePixelResults",
                 &TestRunnerBindings::SetShouldGeneratePixelResults)
      .SetMethod(
          "setShouldStayOnPageAfterHandlingBeforeUnload",
          &TestRunnerBindings::SetShouldStayOnPageAfterHandlingBeforeUnload)
      .SetMethod("setSpellCheckResolvedCallback",
                 &TestRunnerBindings::SetSpellCheckResolvedCallback)
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
    bool forbidden,
    const std::string& scheme) {
  if (view_runner_)
    view_runner_->SetDomainRelaxationForbiddenForURLScheme(forbidden, scheme);
}

void TestRunnerBindings::SetDumpConsoleMessages(bool enabled) {
  if (runner_)
    runner_->SetDumpConsoleMessages(enabled);
}

void TestRunnerBindings::SetDumpJavaScriptDialogs(bool enabled) {
  if (runner_)
    runner_->SetDumpJavaScriptDialogs(enabled);
}

void TestRunnerBindings::SetEffectiveConnectionType(
    const std::string& connection_type) {
  blink::WebEffectiveConnectionType web_type =
      blink::WebEffectiveConnectionType::kTypeUnknown;
  if (connection_type == "TypeUnknown")
    web_type = blink::WebEffectiveConnectionType::kTypeUnknown;
  else if (connection_type == "TypeOffline")
    web_type = blink::WebEffectiveConnectionType::kTypeOffline;
  else if (connection_type == "TypeSlow2G")
    web_type = blink::WebEffectiveConnectionType::kTypeSlow2G;
  else if (connection_type == "Type2G")
    web_type = blink::WebEffectiveConnectionType::kType2G;
  else if (connection_type == "Type3G")
    web_type = blink::WebEffectiveConnectionType::kType3G;
  else if (connection_type == "Type4G")
    web_type = blink::WebEffectiveConnectionType::kType4G;
  else
    NOTREACHED();

  if (runner_)
    runner_->SetEffectiveConnectionType(web_type);
}

void TestRunnerBindings::SetMockSpellCheckerEnabled(bool enabled) {
  if (runner_)
    runner_->SetMockSpellCheckerEnabled(enabled);
}

void TestRunnerBindings::SetSpellCheckResolvedCallback(
    v8::Local<v8::Function> callback) {
  if (runner_)
    runner_->spellcheck_->SetSpellCheckResolvedCallback(callback);
}

void TestRunnerBindings::RemoveSpellCheckResolvedCallback() {
  if (runner_)
    runner_->spellcheck_->RemoveSpellCheckResolvedCallback();
}

v8::Local<v8::Value>
TestRunnerBindings::EvaluateScriptInIsolatedWorldAndReturnValue(
    int world_id,
    const std::string& script) {
  if (!view_runner_ || world_id <= 0 || world_id >= (1 << 29))
    return v8::Local<v8::Value>();
  return view_runner_->EvaluateScriptInIsolatedWorldAndReturnValue(world_id,
                                                                   script);
}

void TestRunnerBindings::EvaluateScriptInIsolatedWorld(
    int world_id,
    const std::string& script) {
  if (view_runner_ && world_id > 0 && world_id < (1 << 29))
    view_runner_->EvaluateScriptInIsolatedWorld(world_id, script);
}

void TestRunnerBindings::SetIsolatedWorldSecurityOrigin(
    int world_id,
    v8::Local<v8::Value> origin) {
  if (view_runner_)
    view_runner_->SetIsolatedWorldSecurityOrigin(world_id, origin);
}

void TestRunnerBindings::SetIsolatedWorldContentSecurityPolicy(
    int world_id,
    const std::string& policy) {
  if (view_runner_)
    view_runner_->SetIsolatedWorldContentSecurityPolicy(world_id, policy);
}

void TestRunnerBindings::AddOriginAccessWhitelistEntry(
    const std::string& source_origin,
    const std::string& destination_protocol,
    const std::string& destination_host,
    bool allow_destination_subdomains) {
  if (runner_) {
    runner_->AddOriginAccessWhitelistEntry(source_origin, destination_protocol,
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
    runner_->RemoveOriginAccessWhitelistEntry(
        source_origin, destination_protocol, destination_host,
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
    return runner_->EnableAutoResizeMode(min_width, min_height, max_width,
                                         max_height);
  }
  return false;
}

bool TestRunnerBindings::DisableAutoResizeMode(int new_width, int new_height) {
  if (runner_)
    return runner_->DisableAutoResizeMode(new_width, new_height);
  return false;
}

void TestRunnerBindings::SetMockDeviceMotion(gin::Arguments* args) {
  if (!runner_)
    return;

  bool has_acceleration_x = false;
  double acceleration_x = 0.0;
  bool has_acceleration_y = false;
  double acceleration_y = 0.0;
  bool has_acceleration_z = false;
  double acceleration_z = 0.0;
  bool has_acceleration_including_gravity_x = false;
  double acceleration_including_gravity_x = 0.0;
  bool has_acceleration_including_gravity_y = false;
  double acceleration_including_gravity_y = 0.0;
  bool has_acceleration_including_gravity_z = false;
  double acceleration_including_gravity_z = 0.0;
  bool has_rotation_rate_alpha = false;
  double rotation_rate_alpha = 0.0;
  bool has_rotation_rate_beta = false;
  double rotation_rate_beta = 0.0;
  bool has_rotation_rate_gamma = false;
  double rotation_rate_gamma = 0.0;
  double interval = false;

  args->GetNext(&has_acceleration_x);
  args->GetNext(&acceleration_x);
  args->GetNext(&has_acceleration_y);
  args->GetNext(&acceleration_y);
  args->GetNext(&has_acceleration_z);
  args->GetNext(&acceleration_z);
  args->GetNext(&has_acceleration_including_gravity_x);
  args->GetNext(&acceleration_including_gravity_x);
  args->GetNext(&has_acceleration_including_gravity_y);
  args->GetNext(&acceleration_including_gravity_y);
  args->GetNext(&has_acceleration_including_gravity_z);
  args->GetNext(&acceleration_including_gravity_z);
  args->GetNext(&has_rotation_rate_alpha);
  args->GetNext(&rotation_rate_alpha);
  args->GetNext(&has_rotation_rate_beta);
  args->GetNext(&rotation_rate_beta);
  args->GetNext(&has_rotation_rate_gamma);
  args->GetNext(&rotation_rate_gamma);
  args->GetNext(&interval);

  runner_->SetMockDeviceMotion(
      has_acceleration_x, acceleration_x, has_acceleration_y, acceleration_y,
      has_acceleration_z, acceleration_z, has_acceleration_including_gravity_x,
      acceleration_including_gravity_x, has_acceleration_including_gravity_y,
      acceleration_including_gravity_y, has_acceleration_including_gravity_z,
      acceleration_including_gravity_z, has_rotation_rate_alpha,
      rotation_rate_alpha, has_rotation_rate_beta, rotation_rate_beta,
      has_rotation_rate_gamma, rotation_rate_gamma, interval);
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

  runner_->SetMockDeviceOrientation(has_alpha, alpha, has_beta, beta, has_gamma,
                                    gamma, absolute);
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

void TestRunnerBindings::SetDisallowedSubresourcePathSuffixes(
    const std::vector<std::string>& suffixes) {
  if (runner_)
    runner_->SetDisallowedSubresourcePathSuffixes(suffixes);
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

void TestRunnerBindings::SetShouldGeneratePixelResults(bool value) {
  if (runner_)
    runner_->setShouldGeneratePixelResults(value);
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

void TestRunnerBindings::SetBlockThirdPartyCookies(bool block) {
  if (runner_)
    runner_->SetBlockThirdPartyCookies(block);
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
    double value,
    v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->SetBackingScaleFactor(value, callback);
}

void TestRunnerBindings::EnableUseZoomForDSF(v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->EnableUseZoomForDSF(callback);
}

void TestRunnerBindings::SetColorProfile(const std::string& name,
                                         v8::Local<v8::Function> callback) {
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
  if (runner_) {
    runner_->SetMIDIAccessorResult(
        result ? midi::mojom::Result::OK
               : midi::mojom::Result::INITIALIZATION_ERROR);
  }
}

void TestRunnerBindings::SimulateWebNotificationClick(gin::Arguments* args) {
  DCHECK_GE(args->Length(), 1);
  if (!runner_)
    return;

  std::string title;
  base::Optional<int> action_index;
  base::Optional<base::string16> reply;

  args->GetNext(&title);

  // Optional |action_index| argument.
  if (args->Length() >= 2) {
    int action_index_int;
    args->GetNext(&action_index_int);

    action_index = action_index_int;
  }

  // Optional |reply| argument.
  if (args->Length() >= 3) {
    std::string reply_string;
    args->GetNext(&reply_string);

    reply = base::UTF8ToUTF16(reply_string);
  }

  runner_->SimulateWebNotificationClick(title, action_index, reply);
}

void TestRunnerBindings::SimulateWebNotificationClose(const std::string& title,
                                                      bool by_user) {
  if (!runner_)
    return;
  runner_->SimulateWebNotificationClose(title, by_user);
}

void TestRunnerBindings::AddMockSpeechRecognitionResult(
    const std::string& transcript,
    double confidence) {
  if (runner_)
    runner_->AddMockSpeechRecognitionResult(transcript, confidence);
}

void TestRunnerBindings::SetMockSpeechRecognitionError(
    const std::string& error,
    const std::string& message) {
  if (runner_)
    runner_->SetMockSpeechRecognitionError(error, message);
}

void TestRunnerBindings::SetMockCredentialManagerResponse(
    const std::string& id,
    const std::string& name,
    const std::string& avatar,
    const std::string& password) {
  if (runner_)
    runner_->SetMockCredentialManagerResponse(id, name, avatar, password);
}

void TestRunnerBindings::ClearMockCredentialManagerResponse() {
  if (runner_)
    runner_->ClearMockCredentialManagerResponse();
}

void TestRunnerBindings::SetMockCredentialManagerError(
    const std::string& error) {
  if (runner_)
    runner_->SetMockCredentialManagerError(error);
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
    int x,
    int y,
    v8::Local<v8::Function> callback) {
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

void TestRunnerBindings::SetPermission(const std::string& name,
                                       const std::string& value,
                                       const std::string& origin,
                                       const std::string& embedding_origin) {
  if (!runner_)
    return;

  return runner_->SetPermission(name, value, GURL(origin),
                                GURL(embedding_origin));
}

void TestRunnerBindings::DispatchBeforeInstallPromptEvent(
    const std::vector<std::string>& event_platforms,
    v8::Local<v8::Function> callback) {
  if (!view_runner_)
    return;

  return view_runner_->DispatchBeforeInstallPromptEvent(event_platforms,
                                                        callback);
}

void TestRunnerBindings::ResolveBeforeInstallPromptPromise(
    const std::string& platform) {
  if (!runner_)
    return;

  runner_->ResolveBeforeInstallPromptPromise(platform);
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

void TestRunnerBindings::ForceNextWebGLContextCreationToFail() {
  if (view_runner_)
    view_runner_->ForceNextWebGLContextCreationToFail();
}

void TestRunnerBindings::ForceNextDrawingBufferCreationToFail() {
  if (view_runner_)
    view_runner_->ForceNextDrawingBufferCreationToFail();
}

void TestRunnerBindings::NotImplemented(const gin::Arguments& args) {}

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
    controller_->delegate_->PostTask(base::Bind(
        &TestRunner::WorkQueue::ProcessWork, weak_factory_.GetWeakPtr()));
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
      is_web_platform_tests_mode_(false),
      effective_connection_type_(
          blink::WebEffectiveConnectionType::kTypeUnknown),
      weak_factory_(this) {}

TestRunner::~TestRunner() {}

void TestRunner::Install(
    WebLocalFrame* frame,
    base::WeakPtr<TestRunnerForSpecificView> view_test_runner) {
  TestRunnerBindings::Install(weak_factory_.GetWeakPtr(), view_test_runner,
                              frame, is_web_platform_tests_mode());
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
  if (disable_v8_cache_)
    SetV8CacheDisabled(true);
}

void TestRunner::Reset() {
  is_web_platform_tests_mode_ = false;
  top_loading_frame_ = nullptr;
  layout_test_runtime_flags_.Reset();
  mock_screen_orientation_client_->ResetData();
  drag_image_.Reset();

  WebSecurityPolicy::ResetOriginAccessWhitelists();
#if defined(__linux__) || defined(ANDROID)
  WebFontRendering::SetSubpixelPositioning(false);
#endif

  if (delegate_) {
    // Reset the default quota for each origin.
    delegate_->SetDatabaseQuota(kDefaultDatabaseQuota);
    delegate_->SetDeviceColorSpace("reset");
    delegate_->SetDeviceScaleFactor(GetDefaultDeviceScaleFactor());
    delegate_->SetBlockThirdPartyCookies(true);
    delegate_->SetLocale("");
    delegate_->UseUnfortunateSynchronousResizeMode(false);
    delegate_->DisableAutoResizeMode(WebSize());
    delegate_->DeleteAllCookies();
    delegate_->SetBluetoothManualChooser(false);
    delegate_->ResetPermissions();
  }

  dump_as_audio_ = false;
  dump_back_forward_list_ = false;
  test_repaint_ = false;
  sweep_horizontally_ = false;
  midi_accessor_result_ = midi::mojom::Result::OK;

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

  spellcheck_->Reset();
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
  return layout_test_runtime_flags_.has_custom_text_output();
}

std::string TestRunner::customDumpText() const {
  return layout_test_runtime_flags_.custom_text_output();
}

void TestRunner::setCustomTextOutput(const std::string& text) {
  layout_test_runtime_flags_.set_custom_text_output(text);
  layout_test_runtime_flags_.set_has_custom_text_output(true);
  OnLayoutTestRuntimeFlagsChanged();
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
    blink::WebLocalFrame* frame,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  if (layout_test_runtime_flags_.dump_drag_image()) {
    if (drag_image_.IsNull()) {
      // This means the test called dumpDragImage but did not initiate a drag.
      // Return a blank image so that the test fails.
      SkBitmap bitmap;
      bitmap.allocN32Pixels(1, 1);
      bitmap.eraseColor(0);
      std::move(callback).Run(bitmap);
      return;
    }

    std::move(callback).Run(drag_image_.GetSkBitmap());
    return;
  }

  // See if we need to draw the selection bounds rect on top of the snapshot.
  if (layout_test_runtime_flags_.dump_selection_rect()) {
    callback =
        CreateSelectionBoundsRectDrawingCallback(frame, std::move(callback));
  }

  // Request appropriate kind of pixel dump.
  if (layout_test_runtime_flags_.is_printing()) {
    test_runner::PrintFrameAsync(frame, std::move(callback));
  } else {
    // TODO(lukasza): Ask the |delegate_| to capture the pixels in the browser
    // process, so that OOPIF pixels are also captured.
    test_runner::DumpPixelsAsync(frame, delegate_->GetDeviceScaleFactor(),
                                 std::move(callback));
  }
}

void TestRunner::ReplicateLayoutTestRuntimeFlagsChanges(
    const base::DictionaryValue& changed_values) {
  if (test_is_running_) {
    layout_test_runtime_flags_.tracked_dictionary().ApplyUntrackedChanges(
        changed_values);

    bool allowed = layout_test_runtime_flags_.plugins_allowed();
    for (WebViewTestProxyBase* window : test_interfaces_->GetWindowList())
      window->web_view()->GetSettings()->SetPluginsEnabled(allowed);
  }
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

  CHECK(main_view_->MainFrame()->IsWebLocalFrame())
      << "This function requires that the main frame is a local frame.";
  main_view_->MainFrame()->ToWebLocalFrame()->EnableViewSourceMode(value);
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
  return layout_test_runtime_flags_.dump_create_view();
}

bool TestRunner::canOpenWindows() const {
  return layout_test_runtime_flags_.can_open_windows();
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

WebTextCheckClient* TestRunner::GetWebTextCheckClient() const {
  return spellcheck_.get();
}

void TestRunner::InitializeWebViewWithMocks(blink::WebView* web_view) {
  web_view->SetCredentialManagerClient(credential_manager_client_.get());
}

bool TestRunner::shouldDumpSpellCheckCallbacks() const {
  return layout_test_runtime_flags_.dump_spell_check_callbacks();
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
  return test_is_running_ && frame->Top()->View() == main_view_;
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

WebFrame* TestRunner::mainFrame() const {
  return main_view_->MainFrame();
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

void TestRunner::setToolTipText(const WebString& text) {
  tooltip_text_ = text.Utf8();
}

void TestRunner::setDragImage(const blink::WebImage& drag_image) {
  if (layout_test_runtime_flags_.dump_drag_image()) {
    if (drag_image_.IsNull())
      drag_image_ = drag_image;
  }
}

bool TestRunner::shouldDumpNavigationPolicy() const {
  return layout_test_runtime_flags_.dump_navigation_policy();
}

midi::mojom::Result TestRunner::midiAccessorResult() {
  return midi_accessor_result_;
}

void TestRunner::ClearDevToolsLocalStorage() {
  delegate_->ClearDevToolsLocalStorage();
}

void TestRunner::SetV8CacheDisabled(bool disabled) {
  if (!main_view_) {
    disable_v8_cache_ = disabled;
    return;
  }
  main_view_->GetSettings()->SetV8CacheOptions(
      disabled ? blink::WebSettings::kV8CacheOptionsNone
               : blink::WebSettings::kV8CacheOptionsDefault);
}

void TestRunner::ShowDevTools(const std::string& settings,
                              const std::string& frontend_url) {
  delegate_->ShowDevTools(settings, frontend_url);
}

class WorkItemBackForward : public TestRunner::WorkItem {
 public:
  explicit WorkItemBackForward(int distance) : distance_(distance) {}

  bool Run(WebTestDelegate* delegate, WebView*) override {
    delegate->GoToOffset(distance_);
    return true;  // FIXME: Did it really start a navigation?
  }

 private:
  int distance_;
};

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
  explicit WorkItemLoadingScript(const std::string& script) : script_(script) {}

  bool Run(WebTestDelegate*, WebView* web_view) override {
    blink::WebFrame* main_frame = web_view->MainFrame();
    if (!main_frame->IsWebLocalFrame()) {
      CHECK(false) << "This function cannot be called if the main frame is not "
                      "a local frame.";
      return false;
    }
    main_frame->ToWebLocalFrame()->ExecuteScript(
        WebScriptSource(WebString::FromUTF8(script_)));
    return true;  // FIXME: Did it really start a navigation?
  }

 private:
  std::string script_;
};

void TestRunner::QueueLoadingScript(const std::string& script) {
  work_queue_.AddWork(new WorkItemLoadingScript(script));
}

class WorkItemNonLoadingScript : public TestRunner::WorkItem {
 public:
  explicit WorkItemNonLoadingScript(const std::string& script)
      : script_(script) {}

  bool Run(WebTestDelegate*, WebView* web_view) override {
    blink::WebFrame* main_frame = web_view->MainFrame();
    if (!main_frame->IsWebLocalFrame()) {
      CHECK(false) << "This function cannot be called if the main frame is not "
                      "a local frame.";
      return false;
    }
    main_frame->ToWebLocalFrame()->ExecuteScript(
        WebScriptSource(WebString::FromUTF8(script_)));
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
    return true;  // FIXME: Did it really start a navigation?
  }

 private:
  WebURL url_;
  std::string target_;
};

void TestRunner::QueueLoad(const std::string& url, const std::string& target) {
  if (!main_view_)
    return;

  // TODO(lukasza): testRunner.queueLoad(...) should work even if the main frame
  // is remote (ideally testRunner.queueLoad would bind to and execute in the
  // context of a specific local frame - resolving relative urls should be done
  // on relative to the calling frame's url).
  CHECK(main_view_->MainFrame()->IsWebLocalFrame())
      << "This function cannot be called if the main frame is not "
         "a local frame.";

  // FIXME: Implement WebURL::resolve() and avoid GURL.
  GURL current_url =
      main_view_->MainFrame()->ToWebLocalFrame()->GetDocument().Url();
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
  if (!url.IsValid())
    return;

  WebSecurityPolicy::AddOriginAccessWhitelistEntry(
      url, WebString::FromUTF8(destination_protocol),
      WebString::FromUTF8(destination_host), allow_destination_subdomains);
}

void TestRunner::RemoveOriginAccessWhitelistEntry(
    const std::string& source_origin,
    const std::string& destination_protocol,
    const std::string& destination_host,
    bool allow_destination_subdomains) {
  WebURL url((GURL(source_origin)));
  if (!url.IsValid())
    return;

  WebSecurityPolicy::RemoveOriginAccessWhitelistEntry(
      url, WebString::FromUTF8(destination_protocol),
      WebString::FromUTF8(destination_host), allow_destination_subdomains);
}

void TestRunner::SetTextSubpixelPositioning(bool value) {
#if defined(__linux__) || defined(ANDROID)
  // Since FontConfig doesn't provide a variable to control subpixel
  // positioning, we'll fall back to setting it globally for all fonts.
  WebFontRendering::SetSubpixelPositioning(value);
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

void TestRunner::SetMockDeviceMotion(bool has_acceleration_x,
                                     double acceleration_x,
                                     bool has_acceleration_y,
                                     double acceleration_y,
                                     bool has_acceleration_z,
                                     double acceleration_z,
                                     bool has_acceleration_including_gravity_x,
                                     double acceleration_including_gravity_x,
                                     bool has_acceleration_including_gravity_y,
                                     double acceleration_including_gravity_y,
                                     bool has_acceleration_including_gravity_z,
                                     double acceleration_including_gravity_z,
                                     bool has_rotation_rate_alpha,
                                     double rotation_rate_alpha,
                                     bool has_rotation_rate_beta,
                                     double rotation_rate_beta,
                                     bool has_rotation_rate_gamma,
                                     double rotation_rate_gamma,
                                     double interval) {
  device::MotionData motion;

  // acceleration
  motion.has_acceleration_x = has_acceleration_x;
  motion.acceleration_x = acceleration_x;
  motion.has_acceleration_y = has_acceleration_y;
  motion.acceleration_y = acceleration_y;
  motion.has_acceleration_z = has_acceleration_z;
  motion.acceleration_z = acceleration_z;

  // accelerationIncludingGravity
  motion.has_acceleration_including_gravity_x =
      has_acceleration_including_gravity_x;
  motion.acceleration_including_gravity_x = acceleration_including_gravity_x;
  motion.has_acceleration_including_gravity_y =
      has_acceleration_including_gravity_y;
  motion.acceleration_including_gravity_y = acceleration_including_gravity_y;
  motion.has_acceleration_including_gravity_z =
      has_acceleration_including_gravity_z;
  motion.acceleration_including_gravity_z = acceleration_including_gravity_z;

  // rotationRate
  motion.has_rotation_rate_alpha = has_rotation_rate_alpha;
  motion.rotation_rate_alpha = rotation_rate_alpha;
  motion.has_rotation_rate_beta = has_rotation_rate_beta;
  motion.rotation_rate_beta = rotation_rate_beta;
  motion.has_rotation_rate_gamma = has_rotation_rate_gamma;
  motion.rotation_rate_gamma = rotation_rate_gamma;

  // interval
  motion.interval = interval;

  delegate_->SetDeviceMotionData(motion);
}

void TestRunner::SetMockDeviceOrientation(bool has_alpha,
                                          double alpha,
                                          bool has_beta,
                                          double beta,
                                          bool has_gamma,
                                          double gamma,
                                          bool absolute) {
  device::OrientationData orientation;

  // alpha
  orientation.has_alpha = has_alpha;
  orientation.alpha = alpha;

  // beta
  orientation.has_beta = has_beta;
  orientation.beta = beta;

  // gamma
  orientation.has_gamma = has_gamma;
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
    orientation = kWebScreenOrientationPortraitPrimary;
  } else if (orientation_str == "portrait-secondary") {
    orientation = kWebScreenOrientationPortraitSecondary;
  } else if (orientation_str == "landscape-primary") {
    orientation = kWebScreenOrientationLandscapePrimary;
  } else {
    DCHECK_EQ("landscape-secondary", orientation_str);
    orientation = kWebScreenOrientationLandscapeSecondary;
  }

  for (WebViewTestProxyBase* window : test_interfaces_->GetWindowList()) {
    WebFrame* main_frame = window->web_view()->MainFrame();
    // TODO(lukasza): Need to make this work for remote frames.
    if (main_frame->IsWebLocalFrame()) {
      mock_screen_orientation_client_->UpdateDeviceOrientation(
          main_frame->ToWebLocalFrame(), orientation);
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
  delegate_->SetPopupBlockingEnabled(block_popups);
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
    v8::Isolate* isolate = blink::MainThreadIsolate();
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
  } else if (key == "WebKitCSSGridLayoutEnabled") {
    prefs->experimental_css_grid_layout_enabled = value->BooleanValue();
  } else if (key == "WebKitHyperlinkAuditingEnabled") {
    prefs->hyperlink_auditing_enabled = value->BooleanValue();
  } else if (key == "WebKitEnableCaretBrowsing") {
    prefs->caret_browsing_enabled = value->BooleanValue();
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
  } else if (key == "WebKitSpatialNavigationEnabled") {
    prefs->spatial_navigation_enabled = value->BooleanValue();
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

  for (WebViewTestProxyBase* window : test_interfaces_->GetWindowList())
    window->web_view()->AcceptLanguagesChanged();
}

void TestRunner::SetPluginsEnabled(bool enabled) {
  delegate_->Preferences()->plugins_enabled = enabled;
  delegate_->ApplyPreferences();
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
  layout_test_runtime_flags_.set_dump_create_view(true);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetCanOpenWindows() {
  layout_test_runtime_flags_.set_can_open_windows(true);
  OnLayoutTestRuntimeFlagsChanged();
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

  for (WebViewTestProxyBase* window : test_interfaces_->GetWindowList())
    window->web_view()->GetSettings()->SetPluginsEnabled(allowed);

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

void TestRunner::SetDisallowedSubresourcePathSuffixes(
    const std::vector<std::string>& suffixes) {
  DCHECK(main_view_);
  if (!main_view_->MainFrame()->IsWebLocalFrame())
    return;
  main_view_->MainFrame()
      ->ToWebLocalFrame()
      ->GetDocumentLoader()
      ->SetSubresourceFilter(new MockWebDocumentSubresourceFilter(suffixes));
}

void TestRunner::DumpSpellCheckCallbacks() {
  layout_test_runtime_flags_.set_dump_spell_check_callbacks(true);
  OnLayoutTestRuntimeFlagsChanged();
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

void TestRunner::SetUseMockTheme(bool use) {
  use_mock_theme_ = use;
  blink::SetMockThemeEnabledForTest(use);
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

void TestRunner::SetDumpConsoleMessages(bool value) {
  layout_test_runtime_flags_.set_dump_console_messages(value);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetDumpJavaScriptDialogs(bool value) {
  layout_test_runtime_flags_.set_dump_javascript_dialogs(value);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::SetEffectiveConnectionType(
    blink::WebEffectiveConnectionType connection_type) {
  effective_connection_type_ = connection_type;
}

void TestRunner::SetMockSpellCheckerEnabled(bool enabled) {
  spellcheck_->SetEnabled(enabled);
}

bool TestRunner::ShouldDumpConsoleMessages() const {
  return layout_test_runtime_flags_.dump_console_messages();
}

bool TestRunner::ShouldDumpJavaScriptDialogs() const {
  return layout_test_runtime_flags_.dump_javascript_dialogs();
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

void TestRunner::SetBlockThirdPartyCookies(bool block) {
  delegate_->SetBlockThirdPartyCookies(block);
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

void TestRunner::SetPermission(const std::string& name,
                               const std::string& value,
                               const GURL& origin,
                               const GURL& embedding_origin) {
  delegate_->SetPermission(name, value, origin, embedding_origin);
}

void TestRunner::ResolveBeforeInstallPromptPromise(
    const std::string& platform) {
  delegate_->ResolveBeforeInstallPromptPromise(platform);
}

void TestRunner::SetPOSIXLocale(const std::string& locale) {
  delegate_->SetLocale(locale);
}

void TestRunner::SetMIDIAccessorResult(midi::mojom::Result result) {
  midi_accessor_result_ = result;
}

void TestRunner::SimulateWebNotificationClick(
    const std::string& title,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply) {
  delegate_->SimulateWebNotificationClick(title, action_index, reply);
}

void TestRunner::SimulateWebNotificationClose(const std::string& title,
                                              bool by_user) {
  delegate_->SimulateWebNotificationClose(title, by_user);
}

void TestRunner::AddMockSpeechRecognitionResult(const std::string& transcript,
                                                double confidence) {
  getMockWebSpeechRecognizer()->AddMockResult(WebString::FromUTF8(transcript),
                                              confidence);
}

void TestRunner::SetMockSpeechRecognitionError(const std::string& error,
                                               const std::string& message) {
  getMockWebSpeechRecognizer()->SetError(WebString::FromUTF8(error),
                                         WebString::FromUTF8(message));
}

void TestRunner::SetMockCredentialManagerResponse(const std::string& id,
                                                  const std::string& name,
                                                  const std::string& avatar,
                                                  const std::string& password) {
  credential_manager_client_->SetResponse(new WebPasswordCredential(
      WebString::FromUTF8(id), WebString::FromUTF8(password),
      WebString::FromUTF8(name), WebURL(GURL(avatar))));
}

void TestRunner::ClearMockCredentialManagerResponse() {
  credential_manager_client_->SetResponse(nullptr);
}

void TestRunner::SetMockCredentialManagerError(const std::string& error) {
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

  if (!main_view_->MainFrame()->IsWebLocalFrame())
    return;

  WebDocumentLoader* document_loader =
      main_view_->MainFrame()->ToWebLocalFrame()->GetDocumentLoader();
  if (!document_loader)
    return;

  std::string mimeType = document_loader->GetResponse().MimeType().Utf8();
  if (mimeType != "text/plain")
    return;

  layout_test_runtime_flags_.set_dump_as_text(true);
  layout_test_runtime_flags_.set_generate_pixel_results(false);
  OnLayoutTestRuntimeFlagsChanged();
}

void TestRunner::NotifyDone() {
  if (layout_test_runtime_flags_.wait_until_done() && !topLoadingFrame() &&
      work_queue_.is_empty())
    delegate_->TestFinished();
  layout_test_runtime_flags_.set_wait_until_done(false);
  OnLayoutTestRuntimeFlagsChanged();
}

}  // namespace test_runner
