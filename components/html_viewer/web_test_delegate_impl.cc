// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_test_delegate_impl.h"

#include "base/time/time.h"
#include "cc/layers/texture_layer.h"
#include "components/test_runner/web_task.h"
#include "components/test_runner/web_test_interfaces.h"
#include "components/test_runner/web_test_proxy.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "url/gurl.h"

namespace html_viewer {

namespace {

class InvokeTaskHelper : public blink::WebThread::Task {
 public:
  InvokeTaskHelper(scoped_ptr<test_runner::WebTask> task)
      : task_(task.Pass()) {}

  // WebThread::Task implementation:
  void run() override { task_->run(); }

 private:
  scoped_ptr<test_runner::WebTask> task_;
};

}  // namespace

WebTestDelegateImpl::WebTestDelegateImpl()
    : test_interfaces_(nullptr), proxy_(nullptr) {
}

WebTestDelegateImpl::~WebTestDelegateImpl() {
}

void WebTestDelegateImpl::ClearEditCommand() {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetEditCommand(const std::string& name,
                                     const std::string& value) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetGamepadProvider(
    test_runner::GamepadController* controller) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetDeviceLightData(const double data) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetDeviceMotionData(
    const blink::WebDeviceMotionData& data) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetDeviceOrientationData(
    const blink::WebDeviceOrientationData& data) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetScreenOrientation(
    const blink::WebScreenOrientationType& orientation) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::ResetScreenOrientation() {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::DidChangeBatteryStatus(
    const blink::WebBatteryStatus& status) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::PrintMessage(const std::string& message) {
  fprintf(stderr, "%s", message.c_str());
}

void WebTestDelegateImpl::PostTask(test_runner::WebTask* task) {
  blink::Platform::current()->currentThread()->postTask(
      blink::WebTraceLocation(__FUNCTION__, __FILE__),
      new InvokeTaskHelper(make_scoped_ptr(task)));
}

void WebTestDelegateImpl::PostDelayedTask(test_runner::WebTask* task,
                                          long long ms) {
  blink::Platform::current()->currentThread()->postDelayedTask(
      blink::WebTraceLocation(__FUNCTION__, __FILE__),
      new InvokeTaskHelper(make_scoped_ptr(task)), ms);
}

blink::WebString WebTestDelegateImpl::RegisterIsolatedFileSystem(
    const blink::WebVector<blink::WebString>& absolute_filenames) {
  NOTIMPLEMENTED();
  return blink::WebString();
}

long long WebTestDelegateImpl::GetCurrentTimeInMillisecond() {
  return base::TimeDelta(base::Time::Now() - base::Time::UnixEpoch())
             .ToInternalValue() /
         base::Time::kMicrosecondsPerMillisecond;
}

blink::WebString WebTestDelegateImpl::GetAbsoluteWebStringFromUTF8Path(
    const std::string& path) {
  NOTIMPLEMENTED();
  return blink::WebString::fromUTF8(path);
}

blink::WebURL WebTestDelegateImpl::LocalFileToDataURL(
    const blink::WebURL& file_url) {
  NOTIMPLEMENTED();
  return blink::WebURL();
}

blink::WebURL WebTestDelegateImpl::RewriteLayoutTestsURL(
    const std::string& utf8_url) {
  return blink::WebURL(GURL(utf8_url));
}

test_runner::TestPreferences* WebTestDelegateImpl::Preferences() {
  return &prefs_;
}

void WebTestDelegateImpl::ApplyPreferences() {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::UseUnfortunateSynchronousResizeMode(bool enable) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::EnableAutoResizeMode(const blink::WebSize& min_size,
                          const blink::WebSize& max_size) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::DisableAutoResizeMode(
    const blink::WebSize& new_size) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::ClearDevToolsLocalStorage() {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::ShowDevTools(const std::string& settings,
                                       const std::string& frontend_url) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::CloseDevTools() {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::EvaluateInWebInspector(long call_id,
                                                 const std::string& script) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::ClearAllDatabases() {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetDatabaseQuota(int quota) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SimulateWebNotificationClick(
    const std::string& title, int action_index) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetDeviceScaleFactor(float factor) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetDeviceColorProfile(const std::string& name) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetBluetoothMockDataSet(const std::string& data_set) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetGeofencingMockProvider(bool service_available) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::ClearGeofencingMockProvider() {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetGeofencingMockPosition(double latitude,
                                                    double longitude) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetFocus(test_runner::WebTestProxyBase* proxy,
                                   bool focus) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetAcceptAllCookies(bool accept) {
  NOTIMPLEMENTED();
}

std::string WebTestDelegateImpl::PathToLocalResource(
    const std::string& resource) {
  NOTIMPLEMENTED();
  return std::string();
}

void WebTestDelegateImpl::SetLocale(const std::string& locale) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::TestFinished() {
  test_interfaces_->SetTestIsRunning(false);
  fprintf(stderr, "%s", proxy_->CaptureTree(false, false).c_str());
}

void WebTestDelegateImpl::CloseRemainingWindows() {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::DeleteAllCookies() {
  NOTIMPLEMENTED();
}

int WebTestDelegateImpl::NavigationEntryCount() {
  NOTIMPLEMENTED();
  return 0;
}

void WebTestDelegateImpl::GoToOffset(int offset) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::Reload() {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::LoadURLForFrame(const blink::WebURL& url,
                                          const std::string& frame_name) {
  NOTIMPLEMENTED();
}

bool WebTestDelegateImpl::AllowExternalPages() {
  NOTIMPLEMENTED();
  return false;
}

std::string WebTestDelegateImpl::DumpHistoryForWindow(
    test_runner::WebTestProxyBase* proxy) {
  NOTIMPLEMENTED();
  return std::string();
}

void WebTestDelegateImpl::FetchManifest(
    blink::WebView* view,
    const GURL& url,
    const base::Callback<void(const blink::WebURLResponse& response,
                              const std::string& data)>& callback) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::SetPermission(const std::string& permission_name,
                   const std::string& permission_value,
                   const GURL& origin,
                   const GURL& embedding_origin) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::ResetPermissions() {
  NOTIMPLEMENTED();
}

scoped_refptr<cc::TextureLayer>
WebTestDelegateImpl::CreateTextureLayerForMailbox(
    cc::TextureLayerClient* client) {
  NOTIMPLEMENTED();
  return nullptr;
}

blink::WebLayer* WebTestDelegateImpl::InstantiateWebLayer(
    scoped_refptr<cc::TextureLayer> layer) {
  NOTIMPLEMENTED();
  return nullptr;
}

cc::SharedBitmapManager* WebTestDelegateImpl::GetSharedBitmapManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebTestDelegateImpl::DispatchBeforeInstallPromptEvent(
    int request_id,
    const std::vector<std::string>& event_platforms,
    const base::Callback<void(bool)>& callback) {
  NOTIMPLEMENTED();
}

void WebTestDelegateImpl::ResolveBeforeInstallPromptPromise(int request_id,
                                       const std::string& platform) {
  NOTIMPLEMENTED();
}

blink::WebPlugin* WebTestDelegateImpl::CreatePluginPlaceholder(
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace html_viewer
