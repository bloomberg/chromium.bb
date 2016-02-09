// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_TEST_DELEGATE_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_TEST_DELEGATE_IMPL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/test_runner/test_preferences.h"
#include "components/test_runner/web_test_delegate.h"

namespace test_runner {
class WebTestInterfaces;
class WebTestProxyBase;
}

namespace html_viewer {

class WebTestDelegateImpl : public test_runner::WebTestDelegate {
 public:
  WebTestDelegateImpl();
  ~WebTestDelegateImpl();

  void set_test_interfaces(test_runner::WebTestInterfaces* test_interfaces) {
    test_interfaces_ = test_interfaces;
  }
  void set_test_proxy(test_runner::WebTestProxyBase* proxy) {
    proxy_ = proxy;
  }
  void set_completion_callback(const base::Closure& callback) {
    completion_callback_ = callback;
  }

 private:
  // From test_runner::WebTestDelegate:
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
      const std::string& path) override;
  blink::WebURL LocalFileToDataURL(const blink::WebURL& file_url) override;
  blink::WebURL RewriteLayoutTestsURL(const std::string& utf8_url) override;
  test_runner::TestPreferences* Preferences() override;
  void ApplyPreferences() override;
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
  void EnableUseZoomForDSF() override;
  void SetDeviceColorProfile(const std::string& name) override;
  void SetBluetoothMockDataSet(const std::string& data_set) override;
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
  void SetPermission(const std::string& permission_name,
                     const std::string& permission_value,
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
  void ResolveBeforeInstallPromptPromise(int request_id,
                                         const std::string& platform) override;
  blink::WebPlugin* CreatePluginPlaceholder(
      blink::WebLocalFrame* frame,
      const blink::WebPluginParams& params) override;
  void OnWebTestProxyBaseDestroy(test_runner::WebTestProxyBase* base) override;
  blink::WebPoint ConvertDIPToNative(
      const blink::WebPoint& point_in_dip) const override;

  // Must not be constructed until after blink has been initialized.
  scoped_ptr<test_runner::TestPreferences> prefs_;
  test_runner::WebTestInterfaces* test_interfaces_;
  test_runner::WebTestProxyBase* proxy_;
  base::Closure completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebTestDelegateImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_TEST_DELEGATE_IMPL_H_
