// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace content {

class OffTheRecordClipboardTest : public ContentBrowserTest {
 protected:
  ui::Clipboard* clipboard() const {
    static ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
    return clipboard;
  }

  void ExpectStringInClipboard(const string16& pattern) {
    string16 content;
    clipboard()->ReadText(ui::Clipboard::BUFFER_STANDARD, &content);
    EXPECT_EQ(pattern, content);
  }

  static void WriteClipboardCallback(ui::Clipboard::Buffer expected,
                                     const base::Closure& closure,
                                     ui::Clipboard::Buffer actual) {
    if (expected == actual)
      closure.Run();
  }
};

// Tests that data copied from content area of OffTheRecord window is destroyed
// after context destruction.
IN_PROC_BROWSER_TEST_F(OffTheRecordClipboardTest, PRE_ClearContentData) {
  Shell* shell_offtherecord = CreateOffTheRecordBrowser();
  NavigateToURL(shell_offtherecord, GURL("data:text/plain,foo"));
  // Clear the clipboard. Currently GTK Clipboard::Clear fails to clear
  // clipboard from external content.
  {
    ui::ScopedClipboardWriter clipboard_writer(clipboard(),
                                               ui::Clipboard::BUFFER_STANDARD);
    clipboard_writer.WriteText(ASCIIToUTF16("bar"));
  }
  ExpectStringInClipboard(ASCIIToUTF16("bar"));

  // Select and copy web-page content.
  WebContents* web_contents = shell_offtherecord->web_contents();
  RenderViewHost* view_host = web_contents->GetRenderViewHost();
  view_host->SelectAll();
  view_host->Copy();

  // Run message loop until data have been copied to the clipboard. This happens
  // on UI thread.
  scoped_refptr<MessageLoopRunner> message_loop_runner(new MessageLoopRunner);
  clipboard()->set_write_objects_callback_for_testing(
      base::Bind(&WriteClipboardCallback,
                 ui::Clipboard::BUFFER_STANDARD,
                 message_loop_runner->QuitClosure()));
  message_loop_runner->Run();
  ExpectStringInClipboard(ASCIIToUTF16("foo"));
}

IN_PROC_BROWSER_TEST_F(OffTheRecordClipboardTest, ClearContentData) {
  EXPECT_FALSE(clipboard()->IsFormatAvailable(
      ui::Clipboard::GetPlainTextFormatType(), ui::Clipboard::BUFFER_STANDARD));
}

}  // namespace content
