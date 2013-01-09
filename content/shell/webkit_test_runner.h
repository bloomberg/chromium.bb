// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_
#define CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebPreferences.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTestDelegate.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTestRunner.h"
#include "v8/include/v8.h"

class SkCanvas;

namespace WebKit {
struct WebRect;
}

namespace WebTestRunner {
class WebTestProxyBase;
}

namespace content {

// This is the renderer side of the webkit test runner.
class WebKitTestRunner : public RenderViewObserver,
                         public WebTestRunner::WebTestDelegate,
                         public WebTestRunner::WebTestRunner {
 public:
  explicit WebKitTestRunner(RenderView* render_view);
  virtual ~WebKitTestRunner();

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidClearWindowObject(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidFinishLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidRequestShowContextMenu(
      WebKit::WebFrame* frame,
      const WebKit::WebContextMenuData& data) OVERRIDE;

  // WebTestDelegate implementation.
  virtual void clearContextMenuData();
  virtual void clearEditCommand();
  virtual void fillSpellingSuggestionList(
      const WebKit::WebString& word,
      WebKit::WebVector<WebKit::WebString>* suggestions);
  virtual void setEditCommand(const std::string& name,
                              const std::string& value);
  virtual WebKit::WebContextMenuData* lastContextMenuData() const;
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

  // WebTestRunner implementation.
  virtual bool shouldDumpEditingCallbacks() const;
  virtual bool shouldDumpFrameLoadCallbacks() const;
  virtual bool shouldDumpUserGestureInFrameLoadCallbacks() const;
  virtual bool stopProvisionalFrameLoads() const;
  virtual bool shouldDumpTitleChanges() const;

  void Reset();
  void Display();
  void SetXSSAuditorEnabled(bool enabled);
  void NotifyDone();
  void DumpAsText();
  void DumpChildFramesAsText();
  void SetPrinting();
  void SetShouldStayOnPageAfterHandlingBeforeUnload(bool should_stay_on_page);
  void WaitUntilDone();
  void CanOpenWindows();
  void ShowWebInspector();
  void CloseWebInspector();
  void EvaluateInWebInspector(int32_t call_id, const std::string& script);
  void ExecCommand(const std::string& command, const std::string& value);
  void OverridePreference(const std::string& key, v8::Local<v8::Value> value);
  void DumpEditingCallbacks();
  void DumpFrameLoadCallbacks();
  void DumpUserGestureInFrameLoadCallbacks();
  void StopProvisionalFrameLoads();
  void DumpTitleChanges();

  void NotImplemented(const std::string& object, const std::string& method);

  void set_proxy(::WebTestRunner::WebTestProxyBase* proxy) { proxy_ = proxy; }

 private:
  // Message handlers.
  void OnCaptureTextDump(bool as_text, bool printing, bool recursive);
  void OnCaptureImageDump(const std::string& expected_pixel_hash);
  void OnSetCurrentWorkingDirectory(const FilePath& current_working_directory);

  SkCanvas* GetCanvas();
  void PaintRect(const WebKit::WebRect& rect);
  void PaintInvalidatedRegion();
  void DisplayRepaintMask();

  scoped_ptr<SkCanvas> canvas_;
  scoped_ptr<WebKit::WebContextMenuData> last_context_menu_data_;
  FilePath current_working_directory_;

  ::WebTestRunner::WebTestProxyBase* proxy_;

  ::WebTestRunner::WebPreferences prefs_;

  bool dump_editing_callbacks_;
  bool dump_frame_load_callbacks_;
  bool dump_user_gesture_in_frame_load_callbacks_;
  bool stop_provisional_frame_loads_;
  bool dump_title_changes_;

  bool test_is_running_;
  bool wait_until_done_;
  bool load_finished_;

  DISALLOW_COPY_AND_ASSIGN(WebKitTestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_
