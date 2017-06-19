// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_BLINK_TEST_RUNNER_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_BLINK_TEST_RUNNER_H_

#include <deque>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/common/page_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "content/shell/common/layout_test.mojom.h"
#include "content/shell/common/layout_test/layout_test_bluetooth_fake_adapter_setter.mojom.h"
#include "content/shell/test_runner/test_preferences.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "v8/include/v8.h"

class SkBitmap;

namespace base {
class DictionaryValue;
}

namespace blink {
class MotionData;
class OrientationData;
class WebURLRequest;
class WebView;
}

namespace test_runner {
class AppBannerService;
}

namespace content {

class LeakDetector;
struct LeakDetectionResult;

// This is the renderer side of the webkit test runner.
// TODO(lukasza): Rename to LayoutTestRenderViewObserver for consistency with
// LayoutTestRenderFrameObserver.
class BlinkTestRunner : public RenderViewObserver,
                        public RenderViewObserverTracker<BlinkTestRunner>,
                        public test_runner::WebTestDelegate {
 public:
  explicit BlinkTestRunner(RenderView* render_view);
  ~BlinkTestRunner() override;

  // RenderViewObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidClearWindowObject(blink::WebLocalFrame* frame) override;
  void Navigate(const GURL& url) override;
  void DidCommitProvisionalLoad(blink::WebLocalFrame* frame,
                                bool is_new_navigation) override;
  void DidFailProvisionalLoad(blink::WebLocalFrame* frame,
                              const blink::WebURLError& error) override;

  // WebTestDelegate implementation.
  void ClearEditCommand() override;
  void SetEditCommand(const std::string& name,
                      const std::string& value) override;
  void SetGamepadProvider(test_runner::GamepadController* controller) override;
  void SetDeviceMotionData(const device::MotionData& data) override;
  void SetDeviceOrientationData(const device::OrientationData& data) override;
  void PrintMessageToStderr(const std::string& message) override;
  void PrintMessage(const std::string& message) override;
  void PostTask(const base::Closure& task) override;
  void PostDelayedTask(const base::Closure& task, long long ms) override;
  blink::WebString RegisterIsolatedFileSystem(
      const blink::WebVector<blink::WebString>& absolute_filenames) override;
  long long GetCurrentTimeInMillisecond() override;
  blink::WebString GetAbsoluteWebStringFromUTF8Path(
      const std::string& utf8_path) override;
  blink::WebURL LocalFileToDataURL(const blink::WebURL& file_url) override;
  blink::WebURL RewriteLayoutTestsURL(const std::string& utf8_url,
                                      bool is_wpt_mode) override;
  test_runner::TestPreferences* Preferences() override;
  void ApplyPreferences() override;
  void SetPopupBlockingEnabled(bool block_popups) override;
  virtual std::string makeURLErrorDescription(const blink::WebURLError& error);
  void UseUnfortunateSynchronousResizeMode(bool enable) override;
  void EnableAutoResizeMode(const blink::WebSize& min_size,
                            const blink::WebSize& max_size) override;
  void DisableAutoResizeMode(const blink::WebSize& new_size) override;
  void ClearDevToolsLocalStorage() override;
  void ShowDevTools(const std::string& settings,
                    const std::string& frontend_url) override;
  void CloseDevTools() override;
  void EvaluateInWebInspector(int call_id, const std::string& script) override;
  std::string EvaluateInWebInspectorOverlay(const std::string& script) override;
  void ClearAllDatabases() override;
  void SetDatabaseQuota(int quota) override;
  void SimulateWebNotificationClick(
      const std::string& title,
      int action_index,
      const base::NullableString16& reply) override;
  void SimulateWebNotificationClose(const std::string& title,
                                    bool by_user) override;
  void SetDeviceScaleFactor(float factor) override;
  void SetDeviceColorProfile(const std::string& name) override;
  float GetWindowToViewportScale() override;
  std::unique_ptr<blink::WebInputEvent> TransformScreenToWidgetCoordinates(
      test_runner::WebWidgetTestProxyBase* web_widget_test_proxy_base,
      const blink::WebInputEvent& event) override;
  test_runner::WebWidgetTestProxyBase* GetWebWidgetTestProxyBase(
      blink::WebLocalFrame* frame) override;
  void EnableUseZoomForDSF() override;
  bool IsUseZoomForDSFEnabled() override;
  void SetBluetoothFakeAdapter(const std::string& adapter_name,
                               const base::Closure& callback) override;
  void SetBluetoothManualChooser(bool enable) override;
  void GetBluetoothManualChooserEvents(
      const base::Callback<void(const std::vector<std::string>&)>& callback)
      override;
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument) override;
  void SetFocus(blink::WebView* web_view, bool focus) override;
  void SetBlockThirdPartyCookies(bool block) override;
  std::string PathToLocalResource(const std::string& resource) override;
  void SetLocale(const std::string& locale) override;
  void OnLayoutTestRuntimeFlagsChanged(
      const base::DictionaryValue& changed_values) override;
  void TestFinished() override;
  void CloseRemainingWindows() override;
  void DeleteAllCookies() override;
  int NavigationEntryCount() override;
  void GoToOffset(int offset) override;
  void Reload() override;
  void LoadURLForFrame(const blink::WebURL& url,
                       const std::string& frame_name) override;
  bool AllowExternalPages() override;
  void FetchManifest(
      blink::WebView* view,
      const GURL& url,
      const base::Callback<void(const blink::WebURLResponse& response,
                                const std::string& data)>& callback) override;
  void SetPermission(const std::string& name,
                     const std::string& value,
                     const GURL& origin,
                     const GURL& embedding_origin) override;
  void ResetPermissions() override;
  bool AddMediaStreamVideoSourceAndTrack(
      blink::WebMediaStream* stream) override;
  bool AddMediaStreamAudioSourceAndTrack(
      blink::WebMediaStream* stream) override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  void DispatchBeforeInstallPromptEvent(
      const std::vector<std::string>& event_platforms,
      const base::Callback<void(bool)>& callback) override;
  void ResolveBeforeInstallPromptPromise(
      const std::string& platform) override;
  blink::WebPlugin* CreatePluginPlaceholder(
    const blink::WebPluginParams& params) override;
  float GetDeviceScaleFactor() const override;
  void RunIdleTasks(const base::Closure& callback) override;
  void ForceTextInputStateUpdate(blink::WebLocalFrame* frame) override;
  bool IsNavigationInitiatedByRenderer(
      const blink::WebURLRequest& request) override;

  // Resets a RenderView to a known state for layout tests. It is used both when
  // a RenderView is created and when reusing an existing RenderView for the
  // next test case.
  // When reusing an existing RenderView, |for_new_test| should be true, which
  // also resets additional state, like the main frame's name and opener.
  void Reset(bool for_new_test);

  void ReportLeakDetectionResult(const LeakDetectionResult& result);

  // Message handlers forwarded by LayoutTestRenderFrameObserver.
  void OnSetTestConfiguration(mojom::ShellTestConfigurationPtr params);
  void OnReplicateTestConfiguration(mojom::ShellTestConfigurationPtr params);
  void OnSetupSecondaryRenderer();

 private:
  // Message handlers.
  void OnSessionHistory(
      const std::vector<int>& routing_ids,
      const std::vector<std::vector<PageState> >& session_histories,
      const std::vector<unsigned>& current_entry_indexes);
  void OnReset();
  void OnTestFinishedInSecondaryRenderer();
  void OnTryLeakDetection();
  void OnReplyBluetoothManualChooserEvents(
      const std::vector<std::string>& events);

  // RenderViewObserver implementation.
  void OnDestruct() override;

  // After finishing the test, retrieves the audio, text, and pixel dumps from
  // the TestRunner library and sends them to the browser process.
  void CaptureDump();
  void OnLayoutDumpCompleted(std::string completed_layout_dump);
  void CaptureDumpContinued();
  void OnPixelsDumpCompleted(const SkBitmap& snapshot);
  void CaptureDumpComplete();
  std::string DumpHistoryForWindow(blink::WebView* web_view);

  mojom::LayoutTestBluetoothFakeAdapterSetter&
  GetBluetoothFakeAdapterSetter();
  mojom::LayoutTestBluetoothFakeAdapterSetterPtr bluetooth_fake_adapter_setter_;

  test_runner::TestPreferences prefs_;

  mojom::ShellTestConfigurationPtr test_config_;

  std::vector<int> routing_ids_;
  std::vector<std::vector<PageState> > session_histories_;
  std::vector<unsigned> current_entry_indexes_;

  std::deque<base::Callback<void(const std::vector<std::string>&)>>
      get_bluetooth_events_callbacks_;

  bool is_main_window_;

  bool focus_on_next_commit_;

  std::unique_ptr<test_runner::AppBannerService> app_banner_service_;

  std::unique_ptr<LeakDetector> leak_detector_;

  DISALLOW_COPY_AND_ASSIGN(BlinkTestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_BLINK_TEST_RUNNER_H_
