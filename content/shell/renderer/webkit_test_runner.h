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
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidClearWindowObject(blink::WebLocalFrame* frame) OVERRIDE;
  virtual void Navigate(const GURL& url) OVERRIDE;
  virtual void DidCommitProvisionalLoad(blink::WebLocalFrame* frame,
                                        bool is_new_navigation) OVERRIDE;
  virtual void DidFailProvisionalLoad(blink::WebLocalFrame* frame,
                                      const blink::WebURLError& error) OVERRIDE;

  // WebTestDelegate implementation.
  virtual void ClearEditCommand() OVERRIDE;
  virtual void SetEditCommand(const std::string& name,
                              const std::string& value) OVERRIDE;
  virtual void SetGamepadProvider(scoped_ptr<RendererGamepadProvider>) OVERRIDE;
  virtual void SetDeviceLightData(const double data) OVERRIDE;
  virtual void SetDeviceMotionData(
      const blink::WebDeviceMotionData& data) OVERRIDE;
  virtual void SetDeviceOrientationData(
      const blink::WebDeviceOrientationData& data) OVERRIDE;
  virtual void SetScreenOrientation(
      const blink::WebScreenOrientationType& orientation) OVERRIDE;
  virtual void ResetScreenOrientation() OVERRIDE;
  virtual void DidChangeBatteryStatus(
      const blink::WebBatteryStatus& status) OVERRIDE;
  virtual void PrintMessage(const std::string& message) OVERRIDE;
  virtual void PostTask(WebTask* task) OVERRIDE;
  virtual void PostDelayedTask(WebTask* task, long long ms) OVERRIDE;
  virtual blink::WebString RegisterIsolatedFileSystem(
      const blink::WebVector<blink::WebString>& absolute_filenames) OVERRIDE;
  virtual long long GetCurrentTimeInMillisecond() OVERRIDE;
  virtual blink::WebString GetAbsoluteWebStringFromUTF8Path(
      const std::string& utf8_path) OVERRIDE;
  virtual blink::WebURL LocalFileToDataURL(
      const blink::WebURL& file_url) OVERRIDE;
  virtual blink::WebURL RewriteLayoutTestsURL(
      const std::string& utf8_url) OVERRIDE;
  virtual TestPreferences* Preferences() OVERRIDE;
  virtual void ApplyPreferences() OVERRIDE;
  virtual std::string makeURLErrorDescription(const blink::WebURLError& error);
  virtual void UseUnfortunateSynchronousResizeMode(bool enable) OVERRIDE;
  virtual void EnableAutoResizeMode(const blink::WebSize& min_size,
                                    const blink::WebSize& max_size) OVERRIDE;
  virtual void DisableAutoResizeMode(const blink::WebSize& new_size) OVERRIDE;
  virtual void ClearDevToolsLocalStorage() OVERRIDE;
  virtual void ShowDevTools(const std::string& settings,
                            const std::string& frontend_url) OVERRIDE;
  virtual void CloseDevTools() OVERRIDE;
  virtual void EvaluateInWebInspector(long call_id,
                                      const std::string& script) OVERRIDE;
  virtual void ClearAllDatabases() OVERRIDE;
  virtual void SetDatabaseQuota(int quota) OVERRIDE;
  virtual blink::WebNotificationPresenter::Permission
      CheckWebNotificationPermission(const GURL& origin) OVERRIDE;
  virtual void GrantWebNotificationPermission(const GURL& origin,
                                              bool permission_granted) OVERRIDE;
  virtual void ClearWebNotificationPermissions() OVERRIDE;
  virtual void SetDeviceScaleFactor(float factor) OVERRIDE;
  virtual void SetDeviceColorProfile(const std::string& name) OVERRIDE;
  virtual void SetFocus(WebTestProxyBase* proxy, bool focus) OVERRIDE;
  virtual void SetAcceptAllCookies(bool accept) OVERRIDE;
  virtual std::string PathToLocalResource(const std::string& resource) OVERRIDE;
  virtual void SetLocale(const std::string& locale) OVERRIDE;
  virtual void TestFinished() OVERRIDE;
  virtual void CloseRemainingWindows() OVERRIDE;
  virtual void DeleteAllCookies() OVERRIDE;
  virtual int NavigationEntryCount() OVERRIDE;
  virtual void GoToOffset(int offset) OVERRIDE;
  virtual void Reload() OVERRIDE;
  virtual void LoadURLForFrame(const blink::WebURL& url,
                               const std::string& frame_name) OVERRIDE;
  virtual bool AllowExternalPages() OVERRIDE;
  virtual std::string DumpHistoryForWindow(WebTestProxyBase* proxy) OVERRIDE;

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
