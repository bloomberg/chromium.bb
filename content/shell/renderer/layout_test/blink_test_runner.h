// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_BLINK_TEST_RUNNER_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_BLINK_TEST_RUNNER_H_

#include <deque>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/test_runner/test_preferences.h"
#include "components/test_runner/web_test_delegate.h"
#include "content/public/common/page_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "content/shell/common/shell_test_configuration.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"
#include "v8/include/v8.h"

class SkBitmap;
class SkCanvas;

namespace blink {
class WebBatteryStatus;
class WebDeviceMotionData;
class WebDeviceOrientationData;
struct WebRect;
}

namespace test_runner {
class WebTestProxyBase;
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
  void SetDeviceLightData(const double data) override;
  void SetDeviceMotionData(const blink::WebDeviceMotionData& data) override;
  void SetDeviceOrientationData(
      const blink::WebDeviceOrientationData& data) override;
  void SetScreenOrientation(
      const blink::WebScreenOrientationType& orientation) override;
  void ResetScreenOrientation() override;
  void DidChangeBatteryStatus(const blink::WebBatteryStatus& status) override;
  void PrintMessage(const std::string& message) override;
  void PostTask(test_runner::WebTask* task) override;
  void PostDelayedTask(test_runner::WebTask* task, long long ms) override;
  blink::WebString RegisterIsolatedFileSystem(
      const blink::WebVector<blink::WebString>& absolute_filenames) override;
  long long GetCurrentTimeInMillisecond() override;
  blink::WebString GetAbsoluteWebStringFromUTF8Path(
      const std::string& utf8_path) override;
  blink::WebURL LocalFileToDataURL(const blink::WebURL& file_url) override;
  blink::WebURL RewriteLayoutTestsURL(const std::string& utf8_url) override;
  test_runner::TestPreferences* Preferences() override;
  void ApplyPreferences() override;
  virtual std::string makeURLErrorDescription(const blink::WebURLError& error);
  void UseUnfortunateSynchronousResizeMode(bool enable) override;
  void EnableAutoResizeMode(const blink::WebSize& min_size,
                            const blink::WebSize& max_size) override;
  void DisableAutoResizeMode(const blink::WebSize& new_size) override;
  void ClearDevToolsLocalStorage() override;
  void ShowDevTools(const std::string& settings,
                    const std::string& frontend_url) override;
  void CloseDevTools() override;
  void EvaluateInWebInspector(long call_id, const std::string& script) override;
  std::string EvaluateInWebInspectorOverlay(const std::string& script) override;
  void ClearAllDatabases() override;
  void SetDatabaseQuota(int quota) override;
  void SimulateWebNotificationClick(const std::string& title,
                                    int action_index) override;
  void SimulateWebNotificationClose(const std::string& title,
                                    bool by_user) override;
  void SetDeviceScaleFactor(float factor) override;
  void SetDeviceColorProfile(const std::string& name) override;
  void EnableUseZoomForDSF() override;
  void SetBluetoothMockDataSet(const std::string& name) override;
  void SetBluetoothManualChooser() override;
  void GetBluetoothManualChooserEvents(
      const base::Callback<void(const std::vector<std::string>&)>& callback)
      override;
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument) override;
  void SetGeofencingMockProvider(bool service_available) override;
  void ClearGeofencingMockProvider() override;
  void SetGeofencingMockPosition(double latitude, double longitude) override;
  void SetFocus(test_runner::WebTestProxyBase* proxy, bool focus) override;
  void SetAcceptAllCookies(bool accept) override;
  std::string PathToLocalResource(const std::string& resource) override;
  void SetLocale(const std::string& locale) override;
  void TestFinished() override;
  void CloseRemainingWindows() override;
  void DeleteAllCookies() override;
  int NavigationEntryCount() override;
  void GoToOffset(int offset) override;
  void Reload() override;
  void LoadURLForFrame(const blink::WebURL& url,
                       const std::string& frame_name) override;
  bool AllowExternalPages() override;
  std::string DumpHistoryForWindow(
      test_runner::WebTestProxyBase* proxy) override;
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
      int request_id,
      const std::vector<std::string>& event_platforms,
      const base::Callback<void(bool)>& callback) override;
  void ResolveBeforeInstallPromptPromise(
      int request_id,
      const std::string& platform) override;
  blink::WebPlugin* CreatePluginPlaceholder(
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params) override;
  void OnWebTestProxyBaseDestroy(test_runner::WebTestProxyBase* proxy) override;
  blink::WebPoint ConvertDIPToNative(
      const blink::WebPoint& point_in_dip) const override;

  void Reset();

  void set_proxy(test_runner::WebTestProxyBase* proxy) { proxy_ = proxy; }
  test_runner::WebTestProxyBase* proxy() const { return proxy_; }

  void ReportLeakDetectionResult(const LeakDetectionResult& result);

 private:
  // Message handlers.
  void OnSetTestConfiguration(const ShellTestConfiguration& params);
  void OnSessionHistory(
      const std::vector<int>& routing_ids,
      const std::vector<std::vector<PageState> >& session_histories,
      const std::vector<unsigned>& current_entry_indexes);
  void OnReset();
  void OnNotifyDone();
  void OnTryLeakDetection();
  void OnReplyBluetoothManualChooserEvents(
      const std::vector<std::string>& events);

  // After finishing the test, retrieves the audio, text, and pixel dumps from
  // the TestRunner library and sends them to the browser process.
  void CaptureDump();
  void OnLayoutDumpCompleted(std::string completed_layout_dump);
  void CaptureDumpContinued();
  void OnPixelsDumpCompleted(const SkBitmap& snapshot);
  void CaptureDumpComplete();

  test_runner::WebTestProxyBase* proxy_;

  RenderView* focused_view_;

  test_runner::TestPreferences prefs_;

  ShellTestConfiguration test_config_;

  std::vector<int> routing_ids_;
  std::vector<std::vector<PageState> > session_histories_;
  std::vector<unsigned> current_entry_indexes_;

  std::deque<base::Callback<void(const std::vector<std::string>&)>>
      get_bluetooth_events_callbacks_;

  bool is_main_window_;

  bool focus_on_next_commit_;

  scoped_ptr<LeakDetector> leak_detector_;

  DISALLOW_COPY_AND_ASSIGN(BlinkTestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_BLINK_TEST_RUNNER_H_
