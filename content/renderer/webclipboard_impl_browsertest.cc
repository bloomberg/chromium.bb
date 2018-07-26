// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

namespace {

class WebClipboardImplTest : public ContentBrowserTest {
 public:
  WebClipboardImplTest() = default;
  ~WebClipboardImplTest() override = default;
};

#if defined(OS_CHROMEOS)
// Test is flaky on ChromeOS (https://crbug.com/867339).
#define MAYBE_PasteRTF DISABLED_PasteRTF
#else
#define MAYBE_PasteRTF PasteRTF
#endif
IN_PROC_BROWSER_TEST_F(WebClipboardImplTest, MAYBE_PasteRTF) {
  BrowserTestClipboardScope clipboard;

  const std::string rtf_content = "{\\rtf1\\ansi Hello, {\\b world.}}";
  clipboard.SetRtf(rtf_content);

  // paste_listener.html takes RTF from the clipboard and sets the title.
  NavigateToURL(shell(), GetTestUrl(".", "paste_listener.html"));
  FrameFocusedObserver focus_observer(shell()->web_contents()->GetMainFrame());
  focus_observer.Wait();

  const base::string16 expected_title = base::UTF8ToUTF16(rtf_content);
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  shell()->web_contents()->Paste();
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

#if (defined(OS_CHROMEOS) && !defined(NDEBUG)) || defined(OS_MACOSX)
// Test is flaky on ChromeOS (dbg) and mac (https://crbug.com/867954).
#define MAYBE_ImageCopy DISABLED_ImageCopy
#else
#define MAYBE_ImageCopy ImageCopy
#endif
IN_PROC_BROWSER_TEST_F(WebClipboardImplTest, MAYBE_ImageCopy) {
  BrowserTestClipboardScope clipboard;
  clipboard.SetText("");

  base::string16 expected_types;
  expected_types = base::ASCIIToUTF16("file;image/png string;text/html");

  NavigateToURL(shell(), GetTestUrl(".", "image_copy_types.html"));
  WebContents* web_contents = shell()->web_contents();
  FrameFocusedObserver focus_observer(web_contents->GetMainFrame());
  focus_observer.Wait();

  // Populate an iframe with an image, and wait for load to complete.
  NavigateIframeToURL(web_contents, "copyme",
                      GetTestUrl(".", "media/blackwhite.png"));

  // Run script in child frame to copy image contents and wait for completion.
  RenderFrameHost* child_frame = ChildFrameAt(web_contents->GetMainFrame(), 0);
  child_frame->ExecuteJavaScriptWithUserGestureForTests(
      base::ASCIIToUTF16("document.execCommand('copy');"
                         "parent.document.title = 'copied';"));
  TitleWatcher watcher1(web_contents, base::ASCIIToUTF16("copied"));
  EXPECT_EQ(base::ASCIIToUTF16("copied"), watcher1.WaitAndGetTitle());

  // Paste and check the types.
  web_contents->Paste();
  TitleWatcher watcher2(web_contents, expected_types);
  EXPECT_EQ(expected_types, watcher2.WaitAndGetTitle());
}

}  // namespace

}  // namespace content
