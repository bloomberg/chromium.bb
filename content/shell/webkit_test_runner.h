// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_
#define CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebPreferences.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTestDelegate.h"
#include "v8/include/v8.h"

class SkCanvas;
struct ShellViewMsg_SetTestConfiguration_Params;

namespace WebKit {
struct WebRect;
}

namespace WebTestRunner {
class WebTestProxyBase;
}

namespace content {

// This is the renderer side of the webkit test runner.
class WebKitTestRunner : public RenderViewObserver,
                         public RenderViewObserverTracker<WebKitTestRunner>,
                         public WebTestRunner::WebTestDelegate {
 public:
  explicit WebKitTestRunner(RenderView* render_view);
  virtual ~WebKitTestRunner();

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidClearWindowObject(WebKit::WebFrame* frame) OVERRIDE;

  // WebTestDelegate implementation.
  virtual void clearEditCommand();
  virtual void setEditCommand(const std::string& name,
                              const std::string& value);
  virtual void setGamepadData(const WebKit::WebGamepads& gamepads);
  virtual void printMessage(const std::string& message);
  virtual void postTask(::WebTestRunner::WebTask* task);
  virtual void postDelayedTask(::WebTestRunner::WebTask* task,
                               long long ms);
  virtual WebKit::WebString registerIsolatedFileSystem(
      const WebKit::WebVector<WebKit::WebString>& absolute_filenames);
  virtual long long getCurrentTimeInMillisecond();
  virtual WebKit::WebString getAbsoluteWebStringFromUTF8Path(
      const std::string& utf8_path);
  virtual WebKit::WebURL localFileToDataURL(const WebKit::WebURL& file_url);
  virtual WebKit::WebURL rewriteLayoutTestsURL(const std::string& utf8_url);
  virtual ::WebTestRunner::WebPreferences* preferences();
  virtual void applyPreferences();
  virtual std::string makeURLErrorDescription(const WebKit::WebURLError& error);
  virtual void setClientWindowRect(const WebKit::WebRect& rect);
  virtual void showDevTools();
  virtual void closeDevTools();
  virtual void evaluateInWebInspector(long call_id, const std::string& script);
  virtual void clearAllDatabases();
  virtual void setDatabaseQuota(int quota);
  virtual void setDeviceScaleFactor(float factor);
  virtual void setFocus(bool focus);
  virtual void setAcceptAllCookies(bool accept);
  virtual std::string pathToLocalResource(const std::string& resource);
  virtual void setLocale(const std::string& locale);
  virtual void setDeviceOrientation(WebKit::WebDeviceOrientation& orientation);
  virtual void didAcquirePointerLock();
  virtual void didNotAcquirePointerLock();
  virtual void didLosePointerLock();
  virtual void setPointerLockWillRespondAsynchronously();
  virtual void setPointerLockWillFailSynchronously();
  virtual int numberOfPendingGeolocationPermissionRequests();
  virtual void setGeolocationPermission(bool allowed);
  virtual void setMockGeolocationPosition(double latitude,
                                          double longitude,
                                          double precision);
  virtual void setMockGeolocationPositionUnavailableError(
      const std::string& message);
  virtual void addMockSpeechInputResult(const std::string& result,
                                        double confidence,
                                        const std::string& language);
  virtual void setMockSpeechInputDumpRect(bool dump_rect);
  virtual void addMockSpeechRecognitionResult(const std::string& transcript,
                                              double confidence);
  virtual void setMockSpeechRecognitionError(const std::string& error,
                                             const std::string& message);
  virtual bool wasMockSpeechRecognitionAborted();
  virtual void testFinished();
  virtual void testTimedOut();
  virtual bool isBeingDebugged();
  virtual int layoutTestTimeout();
  virtual void closeRemainingWindows();
  virtual int navigationEntryCount();
  virtual int windowCount();
  virtual void goToOffset(int offset);
  virtual void reload();
  virtual void loadURLForFrame(const WebKit::WebURL& url,
                               const std::string& frame_name);
  virtual bool allowExternalPages();
  virtual void captureHistoryForWindow(
      size_t windowIndex,
      WebKit::WebVector<WebKit::WebHistoryItem>* history,
      size_t* currentEntryIndex);

  void Reset();

  void set_proxy(::WebTestRunner::WebTestProxyBase* proxy) { proxy_ = proxy; }
  ::WebTestRunner::WebTestProxyBase* proxy() const { return proxy_; }

 private:
  // Message handlers.
  void OnSetTestConfiguration(
      const ShellViewMsg_SetTestConfiguration_Params& params);

  void CaptureDump();

  static int window_count_;

  base::FilePath current_working_directory_;
  base::FilePath temp_path_;

  ::WebTestRunner::WebTestProxyBase* proxy_;

  ::WebTestRunner::WebPreferences prefs_;

  bool enable_pixel_dumping_;
  int layout_test_timeout_;
  bool allow_external_pages_;
  std::string expected_pixel_hash_;

  bool is_main_window_;

  DISALLOW_COPY_AND_ASSIGN(WebKitTestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_
