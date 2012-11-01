// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_
#define CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTestDelegate.h"

namespace content {

// This is the renderer side of the webkit test runner.
class WebKitTestRunner : public RenderViewObserver,
                         public WebTestRunner::WebTestDelegate {
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
  virtual void clearContextMenuData() OVERRIDE;
  virtual void clearEditCommand() OVERRIDE;
  virtual void fillSpellingSuggestionList(
      const WebKit::WebString& word,
      WebKit::WebVector<WebKit::WebString>* suggestions) OVERRIDE;
  virtual void setEditCommand(const std::string& name,
                              const std::string& value) OVERRIDE;
  virtual WebKit::WebContextMenuData* lastContextMenuData() const OVERRIDE;
  virtual void setGamepadData(const WebKit::WebGamepads& gamepads) OVERRIDE;
  virtual void printMessage(const std::string& message) OVERRIDE;
  virtual void postTask(WebTestRunner::WebTask* task) OVERRIDE;
  virtual void postDelayedTask(WebTestRunner::WebTask* task,
                               long long ms) OVERRIDE;
  virtual WebKit::WebString registerIsolatedFileSystem(
      const WebKit::WebVector<WebKit::WebString>& absolute_filenames) OVERRIDE;
  virtual long long getCurrentTimeInMillisecond() OVERRIDE;
  virtual WebKit::WebString getAbsoluteWebStringFromUTF8Path(
      const std::string& utf8_path) OVERRIDE;

 private:
  // Message handlers.
  void OnCaptureTextDump(bool as_text, bool printing, bool recursive);
  void OnCaptureImageDump(const std::string& expected_pixel_hash);
  void OnSetIsMainWindow();

  scoped_ptr<WebKit::WebContextMenuData> last_context_menu_data_;
  bool is_main_window_;

  DISALLOW_COPY_AND_ASSIGN(WebKitTestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_
