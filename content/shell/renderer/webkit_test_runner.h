// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_
#define CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/common/page_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "content/shell/common/shell_test_configuration.h"
#include "content/shell/common/test_runner/test_preferences.h"
#include "content/shell/renderer/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationType.h"
#include "v8/include/v8.h"

class SkBitmap;
class SkCanvas;

namespace blink {
class WebBatteryStatus;
class WebDeviceMotionData;
class WebDeviceOrientationData;
struct WebRect;
}

namespace content {

class LeakDetector;
class WebTestProxyBase;
struct LeakDetectionResult;

// This is the renderer side of the webkit test runner.
class WebKitTestRunner : public RenderViewObserver,
                         public RenderViewObserverTracker<WebKitTestRunner>,
                         public WebTestDelegate {
 public:
  explicit WebKitTestRunner(RenderView* render_view);
  virtual ~WebKitTestRunner();

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) override;
  virtual void DidClearWindowObject(blink::WebLocalFrame* frame) override;
  virtual void Navigate(const GURL& url) override;
  virtual void DidCommitProvisionalLoad(blink::WebLocalFrame* frame,
                                        bool is_new_navigation) override;
  virtual void DidFailProvisionalLoad(blink::WebLocalFrame* frame,
                                      const blink::WebURLError& error) override;

  // WebTestDelegate implementation.
  virtual void ClearEditCommand() override;
  virtual void SetEditCommand(const std::string& name,
                              const std::string& value) override;
  virtual void SetGamepadProvider(scoped_ptr<RendererGamepadProvider>) override;
  virtual void SetDeviceLightData(const double data) override;
  virtual void SetDeviceMotionData(
      const blink::WebDeviceMotionData& data) override;
  virtual void SetDeviceOrientationData(
      const blink::WebDeviceOrientationData& data) override;
  virtual void SetScreenOrientation(
      const blink::WebScreenOrientationType& orientation) override;
  virtual void ResetScreenOrientation() override;
  virtual void DidChangeBatteryStatus(
      const blink::WebBatteryStatus& status) override;
  virtual void PrintMessage(const std::string& message) override;
  virtual void PostTask(WebTask* task) override;
  virtual void PostDelayedTask(WebTask* task, long long ms) override;
  virtual blink::WebString RegisterIsolatedFileSystem(
      const blink::WebVector<blink::WebString>& absolute_filenames) override;
  virtual long long GetCurrentTimeInMillisecond() override;
  virtual blink::WebString GetAbsoluteWebStringFromUTF8Path(
      const std::string& utf8_path) override;
  virtual blink::WebURL LocalFileToDataURL(
      const blink::WebURL& file_url) override;
  virtual blink::WebURL RewriteLayoutTestsURL(
      const std::string& utf8_url) override;
  virtual TestPreferences* Preferences() override;
  virtual void ApplyPreferences() override;
  virtual std::string makeURLErrorDescription(const blink::WebURLError& error);
  virtual void UseUnfortunateSynchronousResizeMode(bool enable) override;
  virtual void EnableAutoResizeMode(const blink::WebSize& min_size,
                                    const blink::WebSize& max_size) override;
  virtual void DisableAutoResizeMode(const blink::WebSize& new_size) override;
  virtual void ClearDevToolsLocalStorage() override;
  virtual void ShowDevTools(const std::string& settings,
                            const std::string& frontend_url) override;
  virtual void CloseDevTools() override;
  virtual void EvaluateInWebInspector(long call_id,
                                      const std::string& script) override;
  virtual void ClearAllDatabases() override;
  virtual void SetDatabaseQuota(int quota) override;
  virtual blink::WebNotificationPresenter::Permission
      CheckWebNotificationPermission(const GURL& origin) override;
  virtual void GrantWebNotificationPermission(const GURL& origin,
                                              bool permission_granted) override;
  virtual void ClearWebNotificationPermissions() override;
  virtual void SetDeviceScaleFactor(float factor) override;
  virtual void SetDeviceColorProfile(const std::string& name) override;
  virtual void SetFocus(WebTestProxyBase* proxy, bool focus) override;
  virtual void SetAcceptAllCookies(bool accept) override;
  virtual std::string PathToLocalResource(const std::string& resource) override;
  virtual void SetLocale(const std::string& locale) override;
  virtual void TestFinished() override;
  virtual void CloseRemainingWindows() override;
  virtual void DeleteAllCookies() override;
  virtual int NavigationEntryCount() override;
  virtual void GoToOffset(int offset) override;
  virtual void Reload() override;
  virtual void LoadURLForFrame(const blink::WebURL& url,
                               const std::string& frame_name) override;
  virtual bool AllowExternalPages() override;
  virtual std::string DumpHistoryForWindow(WebTestProxyBase* proxy) override;

  void Reset();

  void set_proxy(WebTestProxyBase* proxy) { proxy_ = proxy; }
  WebTestProxyBase* proxy() const { return proxy_; }

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

  // After finishing the test, retrieves the audio, text, and pixel dumps from
  // the TestRunner library and sends them to the browser process.
  void CaptureDump();
  void CaptureDumpPixels(const SkBitmap& snapshot);
  void CaptureDumpComplete();

  WebTestProxyBase* proxy_;

  RenderView* focused_view_;

  TestPreferences prefs_;

  ShellTestConfiguration test_config_;

  std::vector<int> routing_ids_;
  std::vector<std::vector<PageState> > session_histories_;
  std::vector<unsigned> current_entry_indexes_;

  bool is_main_window_;

  bool focus_on_next_commit_;

  scoped_ptr<LeakDetector> leak_detector_;
  bool needs_leak_detector_;

  DISALLOW_COPY_AND_ASSIGN(WebKitTestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_
