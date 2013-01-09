// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_WEBKIT_TEST_CONTROLLER_H_
#define CONTENT_SHELL_WEBKIT_TEST_CONTROLLER_H_

#include <ostream>
#include <string>

#include "base/cancelable_callback.h"
#include "base/file_path.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "webkit/glue/webpreferences.h"

class SkBitmap;

namespace content {

class Shell;

class WebKitTestResultPrinter {
 public:
  WebKitTestResultPrinter(std::ostream* output, std::ostream* error);
  ~WebKitTestResultPrinter();

  void reset() {
    state_ = BEFORE_TEST;
  }
  bool in_text_block() const { return state_ == IN_TEXT_BLOCK; }
  void set_capture_text_only(bool capture_text_only) {
    capture_text_only_ = capture_text_only;
  }

  void PrintTextHeader();
  void PrintTextBlock(const std::string& block);
  void PrintTextFooter();

  void PrintImageHeader(const std::string& actual_hash,
                        const std::string& expected_hash);
  void PrintImageBlock(const std::vector<unsigned char>& png_image);
  void PrintImageFooter();

  void AddMessage(const std::string& message);
  void AddMessageRaw(const std::string& message);
  void AddErrorMessage(const std::string& message);

 private:
  enum State {
    BEFORE_TEST,
    IN_TEXT_BLOCK,
    IN_IMAGE_BLOCK,
    AFTER_TEST
  };
  State state_;
  bool capture_text_only_;

  std::ostream* output_;
  std::ostream* error_;

  DISALLOW_COPY_AND_ASSIGN(WebKitTestResultPrinter);
};

class WebKitTestController : public base::NonThreadSafe,
                             public WebContentsObserver,
                             public NotificationObserver {
 public:
  static WebKitTestController* Get();

  WebKitTestController();
  virtual ~WebKitTestController();

  // True if the controller is ready for testing.
  bool PrepareForLayoutTest(const GURL& test_url,
                            const FilePath& current_working_directory,
                            bool enable_pixel_dumping,
                            const std::string& expected_pixel_hash);
  // True if the controller was reset successfully.
  bool ResetAfterLayoutTest();

  void RendererUnresponsive();
  void OverrideWebkitPrefs(webkit_glue::WebPreferences* prefs);

  WebKitTestResultPrinter* printer() { return printer_.get(); }
  void set_printer(WebKitTestResultPrinter* printer) {
    printer_.reset(printer);
  }
  bool should_stay_on_page_after_handling_before_unload() const {
    return should_stay_on_page_after_handling_before_unload_;
  }

  // This method can be invoked on any thread.
  bool CanOpenWindows() const;

  // WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void PluginCrashed(const FilePath& plugin_path) OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  static WebKitTestController* instance_;

  void CaptureDump();
  void TimeoutHandler();

  // Message handlers.
  void OnDidFinishLoad();
  void OnImageDump(const std::string& actual_pixel_hash, const SkBitmap& image);
  void OnTextDump(const std::string& dump);
  void OnPrintMessage(const std::string& message);
  void OnReadFileToString(const FilePath& file_path, std::string* contents);
  void OnOverridePreferences(const webkit_glue::WebPreferences& prefs);
  void OnNotifyDone();
  void OnDumpAsText();
  void OnDumpChildFramesAsText();
  void OnSetPrinting();
  void OnSetShouldStayOnPageAfterHandlingBeforeUnload(bool should_stay_on_page);
  void OnWaitUntilDone();
  void OnCanOpenWindows();
  void OnShowWebInspector();
  void OnCloseWebInspector();
  void OnRegisterIsolatedFileSystem(
      const std::vector<FilePath>& absolute_filenames,
      std::string* filesystem_id);

  void OnNotImplemented(const std::string& object_name,
                        const std::string& method_name);

  scoped_ptr<WebKitTestResultPrinter> printer_;

  FilePath current_working_directory_;

  Shell* main_window_;

  int current_pid_;

  bool enable_pixel_dumping_;
  std::string expected_pixel_hash_;

  bool captured_dump_;

  bool dump_as_text_;
  bool dump_child_frames_;
  bool is_printing_;
  bool should_stay_on_page_after_handling_before_unload_;
  bool wait_until_done_;
  bool did_finish_load_;

  webkit_glue::WebPreferences prefs_;
  bool should_override_prefs_;

  base::CancelableClosure watchdog_;

  // Access to the following variables needs to be guarded by |lock_|.
  mutable base::Lock lock_;
  bool can_open_windows_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebKitTestController);
};

}  // namespace content

#endif  // CONTENT_SHELL_WEBKIT_TEST_CONTROLLER_H_
