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
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
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
  virtual void clearEditCommand() OVERRIDE;
  virtual void setEditCommand(const std::string& name,
                              const std::string& value) OVERRIDE;
  virtual void setGamepadProvider(RendererGamepadProvider*) OVERRIDE;
  virtual void setDeviceLightData(const double data) OVERRIDE;
  virtual void setDeviceMotionData(
      const blink::WebDeviceMotionData& data) OVERRIDE;
  virtual void setDeviceOrientationData(
      const blink::WebDeviceOrientationData& data) OVERRIDE;
  virtual void setScreenOrientation(
      const blink::WebScreenOrientationType& orientation) OVERRIDE;
  virtual void resetScreenOrientation() OVERRIDE;
  virtual void didChangeBatteryStatus(
      const blink::WebBatteryStatus& status) OVERRIDE;
  virtual void printMessage(const std::string& message) OVERRIDE;
  virtual void postTask(WebTask* task) OVERRIDE;
  virtual void postDelayedTask(WebTask* task, long long ms) OVERRIDE;
  virtual blink::WebString registerIsolatedFileSystem(
      const blink::WebVector<blink::WebString>& absolute_filenames) OVERRIDE;
  virtual long long getCurrentTimeInMillisecond() OVERRIDE;
  virtual blink::WebString getAbsoluteWebStringFromUTF8Path(
      const std::string& utf8_path) OVERRIDE;
  virtual blink::WebURL localFileToDataURL(
      const blink::WebURL& file_url) OVERRIDE;
  virtual blink::WebURL rewriteLayoutTestsURL(
      const std::string& utf8_url) OVERRIDE;
  virtual TestPreferences* preferences() OVERRIDE;
  virtual void applyPreferences() OVERRIDE;
  virtual std::string makeURLErrorDescription(const blink::WebURLError& error);
  virtual void useUnfortunateSynchronousResizeMode(bool enable) OVERRIDE;
  virtual void enableAutoResizeMode(const blink::WebSize& min_size,
                                    const blink::WebSize& max_size) OVERRIDE;
  virtual void disableAutoResizeMode(const blink::WebSize& new_size) OVERRIDE;
  virtual void clearDevToolsLocalStorage() OVERRIDE;
  virtual void showDevTools(const std::string& settings,
                            const std::string& frontend_url) OVERRIDE;
  virtual void closeDevTools() OVERRIDE;
  virtual void evaluateInWebInspector(long call_id,
                                      const std::string& script) OVERRIDE;
  virtual void clearAllDatabases() OVERRIDE;
  virtual void setDatabaseQuota(int quota) OVERRIDE;
  virtual void setDeviceScaleFactor(float factor) OVERRIDE;
  virtual void setDeviceColorProfile(const std::string& name) OVERRIDE;
  virtual void setFocus(WebTestProxyBase* proxy, bool focus) OVERRIDE;
  virtual void setAcceptAllCookies(bool accept) OVERRIDE;
  virtual std::string pathToLocalResource(const std::string& resource) OVERRIDE;
  virtual void setLocale(const std::string& locale) OVERRIDE;
  virtual void testFinished() OVERRIDE;
  virtual void closeRemainingWindows() OVERRIDE;
  virtual void deleteAllCookies() OVERRIDE;
  virtual int navigationEntryCount() OVERRIDE;
  virtual void goToOffset(int offset) OVERRIDE;
  virtual void reload() OVERRIDE;
  virtual void loadURLForFrame(const blink::WebURL& url,
                               const std::string& frame_name) OVERRIDE;
  virtual bool allowExternalPages() OVERRIDE;
  virtual std::string dumpHistoryForWindow(WebTestProxyBase* proxy) OVERRIDE;

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
