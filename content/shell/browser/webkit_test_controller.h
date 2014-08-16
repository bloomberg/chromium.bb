// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEBKIT_TEST_CONTROLLER_H_
#define CONTENT_SHELL_BROWSER_WEBKIT_TEST_CONTROLLER_H_

#include <ostream>
#include <string>

#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/web_preferences.h"
#include "content/shell/common/leak_detection_result.h"
#include "ui/gfx/size.h"

#if defined(OS_ANDROID)
#include "base/threading/thread_restrictions.h"
#endif

class SkBitmap;

namespace content {

class Shell;

#if defined(OS_ANDROID)
// Android uses a nested message loop for running layout tests because the
// default message loop, provided by the system, does not offer a blocking
// Run() method. The loop itself, implemented as NestedMessagePumpAndroid,
// uses a base::WaitableEvent allowing it to sleep until more events arrive.
class ScopedAllowWaitForAndroidLayoutTests {
 private:
  base::ThreadRestrictions::ScopedAllowWait wait;
};
#endif

class WebKitTestResultPrinter {
 public:
  WebKitTestResultPrinter(std::ostream* output, std::ostream* error);
  ~WebKitTestResultPrinter();

  void reset() {
    state_ = DURING_TEST;
  }
  bool output_finished() const { return state_ == AFTER_TEST; }
  void set_capture_text_only(bool capture_text_only) {
    capture_text_only_ = capture_text_only;
  }

  void set_encode_binary_data(bool encode_binary_data) {
    encode_binary_data_ = encode_binary_data;
  }

  void PrintTextHeader();
  void PrintTextBlock(const std::string& block);
  void PrintTextFooter();

  void PrintImageHeader(const std::string& actual_hash,
                        const std::string& expected_hash);
  void PrintImageBlock(const std::vector<unsigned char>& png_image);
  void PrintImageFooter();

  void PrintAudioHeader();
  void PrintAudioBlock(const std::vector<unsigned char>& audio_data);
  void PrintAudioFooter();

  void AddMessage(const std::string& message);
  void AddMessageRaw(const std::string& message);
  void AddErrorMessage(const std::string& message);

  void CloseStderr();

 private:
  void PrintEncodedBinaryData(const std::vector<unsigned char>& data);

  enum State {
    DURING_TEST,
    IN_TEXT_BLOCK,
    IN_AUDIO_BLOCK,
    IN_IMAGE_BLOCK,
    AFTER_TEST
  };
  State state_;

  bool capture_text_only_;
  bool encode_binary_data_;

  std::ostream* output_;
  std::ostream* error_;

  DISALLOW_COPY_AND_ASSIGN(WebKitTestResultPrinter);
};

class WebKitTestController : public base::NonThreadSafe,
                             public WebContentsObserver,
                             public NotificationObserver,
                             public GpuDataManagerObserver {
 public:
  static WebKitTestController* Get();

  WebKitTestController();
  virtual ~WebKitTestController();

  // True if the controller is ready for testing.
  bool PrepareForLayoutTest(const GURL& test_url,
                            const base::FilePath& current_working_directory,
                            bool enable_pixel_dumping,
                            const std::string& expected_pixel_hash);
  // True if the controller was reset successfully.
  bool ResetAfterLayoutTest();

  void SetTempPath(const base::FilePath& temp_path);
  void RendererUnresponsive();
  void WorkerCrashed();
  void OverrideWebkitPrefs(WebPreferences* prefs);
  void OpenURL(const GURL& url);
  void TestFinishedInSecondaryWindow();
  bool IsMainWindow(WebContents* web_contents) const;

  WebKitTestResultPrinter* printer() { return printer_.get(); }
  void set_printer(WebKitTestResultPrinter* printer) {
    printer_.reset(printer);
  }

  void DevToolsProcessCrashed();

  // WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void PluginCrashed(const base::FilePath& plugin_path,
                             base::ProcessId plugin_pid) OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // GpuDataManagerObserver implementation.
  virtual void OnGpuProcessCrashed(base::TerminationStatus exit_code) OVERRIDE;

 private:
  enum TestPhase {
    BETWEEN_TESTS,
    DURING_TEST,
    CLEAN_UP
  };

  static WebKitTestController* instance_;

  void DiscardMainWindow();
  void SendTestConfiguration();

  // Message handlers.
  void OnAudioDump(const std::vector<unsigned char>& audio_dump);
  void OnImageDump(const std::string& actual_pixel_hash, const SkBitmap& image);
  void OnTextDump(const std::string& dump);
  void OnPrintMessage(const std::string& message);
  void OnOverridePreferences(const WebPreferences& prefs);
  void OnTestFinished();
  void OnClearDevToolsLocalStorage();
  void OnShowDevTools(const std::string& settings,
                      const std::string& frontend_url);
  void OnCloseDevTools();
  void OnGoToOffset(int offset);
  void OnReload();
  void OnLoadURLForFrame(const GURL& url, const std::string& frame_name);
  void OnCaptureSessionHistory();
  void OnCloseRemainingWindows();
  void OnResetDone();
  void OnLeakDetectionDone(const content::LeakDetectionResult& result);

  scoped_ptr<WebKitTestResultPrinter> printer_;

  base::FilePath current_working_directory_;
  base::FilePath temp_path_;

  Shell* main_window_;

  // The PID of the render process of the render view host of main_window_.
  int current_pid_;

  // True if we should set the test configuration to the next RenderViewHost
  // created.
  bool send_configuration_to_next_host_;

  // What phase of running an individual test we are currently in.
  TestPhase test_phase_;

  // True if the currently running test is a compositing test.
  bool is_compositing_test_;

  // Per test config.
  bool enable_pixel_dumping_;
  std::string expected_pixel_hash_;
  gfx::Size initial_size_;
  GURL test_url_;

  // True if the WebPreferences of newly created RenderViewHost should be
  // overridden with prefs_.
  bool should_override_prefs_;
  WebPreferences prefs_;

  NotificationRegistrar registrar_;

  const bool is_leak_detection_enabled_;
  bool crash_when_leak_found_;

#if defined(OS_ANDROID)
  // Because of the nested message pump implementation, Android needs to allow
  // waiting on the UI thread while layout tests are being ran.
  ScopedAllowWaitForAndroidLayoutTests reduced_restrictions_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WebKitTestController);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEBKIT_TEST_CONTROLLER_H_
