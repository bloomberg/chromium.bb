// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "chrome/renderer/print_web_view_helper.h"
#include "chrome/test/render_view_test.h"
#include "printing/image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebView;

namespace {

const char kPrintWithJSHTML[] =
    "<body>Hello<script>window.print()</script>World</body>";

}  // namespace

// Tests that printing pages work and sending and receiving messages through
// that channel all works.
TEST_F(RenderViewTest, OnPrintPages) {
  // Lets simulate a print pages with Hello world.
  LoadHTML("<body><p>Hello World!</p></body>");
  PrintWebViewHelper::Get(view_)->OnPrintPages();

  VerifyPageCount(1);
  VerifyPagesPrinted(true);
}

// Duplicate of OnPrintPagesTest only using javascript to print.
TEST_F(RenderViewTest, PrintWithJavascript) {
  // HTML contains a call to window.print()
  LoadHTML(kPrintWithJSHTML);

  VerifyPageCount(1);
  VerifyPagesPrinted(true);
}

// Tests that the renderer blocks window.print() calls if they occur too
// frequently.
TEST_F(RenderViewTest, BlockScriptInitiatedPrinting) {
  // Pretend user will cancel printing.
  render_thread_.set_print_dialog_user_response(false);
  // Try to print with window.print() a few times.
  LoadHTML(kPrintWithJSHTML);
  LoadHTML(kPrintWithJSHTML);
  LoadHTML(kPrintWithJSHTML);
  VerifyPagesPrinted(false);

  // Pretend user will print. (but printing is blocked.)
  render_thread_.set_print_dialog_user_response(true);
  LoadHTML(kPrintWithJSHTML);
  VerifyPagesPrinted(false);

  // Unblock script initiated printing and verify printing works.
  PrintWebViewHelper::Get(view_)->ResetScriptedPrintCount();
  render_thread_.printer()->ResetPrinter();
  LoadHTML(kPrintWithJSHTML);
  VerifyPageCount(1);
  VerifyPagesPrinted(true);
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// TODO(estade): I don't think this test is worth porting to Linux. We will have
// to rip out and replace most of the IPC code if we ever plan to improve
// printing, and the comment below by sverrir suggests that it doesn't do much
// for us anyway.
TEST_F(RenderViewTest, PrintWithIframe) {
  // Document that populates an iframe.
  const char html[] =
      "<html><body>Lorem Ipsum:"
      "<iframe name=\"sub1\" id=\"sub1\"></iframe><script>"
      "  document.write(frames['sub1'].name);"
      "  frames['sub1'].document.write("
      "      '<p>Cras tempus ante eu felis semper luctus!</p>');"
      "</script></body></html>";

  LoadHTML(html);

  // Find the frame and set it as the focused one.  This should mean that that
  // the printout should only contain the contents of that frame.
  WebFrame* sub1_frame =
      view_->webview()->findFrameByName(WebString::fromUTF8("sub1"));
  ASSERT_TRUE(sub1_frame);
  view_->webview()->setFocusedFrame(sub1_frame);
  ASSERT_NE(view_->webview()->focusedFrame(),
            view_->webview()->mainFrame());

  // Initiate printing.
  PrintWebViewHelper::Get(view_)->OnPrintPages();

  // Verify output through MockPrinter.
  const MockPrinter* printer(render_thread_.printer());
  ASSERT_EQ(1, printer->GetPrintedPages());
  const printing::Image& image1(printer->GetPrintedPage(0)->image());

  // TODO(sverrir): Figure out a way to improve this test to actually print
  // only the content of the iframe.  Currently image1 will contain the full
  // page.
  EXPECT_NE(0, image1.size().width());
  EXPECT_NE(0, image1.size().height());
}
#endif

// Tests if we can print a page and verify its results.
// This test prints HTML pages into a pseudo printer and check their outputs,
// i.e. a simplified version of the PrintingLayoutTextTest UI test.
namespace {
// Test cases used in this test.
struct TestPageData {
  const char* page;
  size_t printed_pages;
  int width;
  int height;
  const char* checksum;
  const wchar_t* file;
};

const TestPageData kTestPages[] = {
  {"<html>"
  "<head>"
  "<meta"
  "  http-equiv=\"Content-Type\""
  "  content=\"text/html; charset=utf-8\"/>"
  "<title>Test 1</title>"
  "</head>"
  "<body style=\"background-color: white;\">"
  "<p style=\"font-family: arial;\">Hello World!</p>"
  "</body>",
#if defined(OS_MACOSX)
  // Mac printing code compensates for the WebKit scale factor while generating
  // the metafile, so we expect smaller pages.
  1, 540, 720,
#else
  1, 675, 900,
#endif
  NULL,
  NULL,
  },
};
}  // namespace

// TODO(estade): need to port MockPrinter to get this on Linux. This involves
// hooking up Cairo to read a pdf stream, or accessing the cairo surface in the
// metafile directly.
#if defined(OS_WIN) || defined(OS_MACOSX)
TEST_F(RenderViewTest, PrintLayoutTest) {
  bool baseline = false;

  EXPECT_TRUE(render_thread_.printer() != NULL);
  for (size_t i = 0; i < arraysize(kTestPages); ++i) {
    // Load an HTML page and print it.
    LoadHTML(kTestPages[i].page);
    PrintWebViewHelper::Get(view_)->OnPrintPages();

    // MockRenderThread::Send() just calls MockRenderThread::OnMsgReceived().
    // So, all IPC messages sent in the above RenderView::OnPrintPages() call
    // has been handled by the MockPrinter object, i.e. this printing job
    // has been already finished.
    // So, we can start checking the output pages of this printing job.
    // Retrieve the number of pages actually printed.
    size_t pages = render_thread_.printer()->GetPrintedPages();
    EXPECT_EQ(kTestPages[i].printed_pages, pages);

    // Retrieve the width and height of the output page.
    int width = render_thread_.printer()->GetWidth(0);
    int height = render_thread_.printer()->GetHeight(0);

    // Check with margin for error.  This has been failing with a one pixel
    // offset on our buildbot.
    const int kErrorMargin = 5;  // 5%
    EXPECT_GT(kTestPages[i].width * (100 + kErrorMargin) / 100, width);
    EXPECT_LT(kTestPages[i].width * (100 - kErrorMargin) / 100, width);
    EXPECT_GT(kTestPages[i].height * (100 + kErrorMargin) / 100, height);
    EXPECT_LT(kTestPages[i].height* (100 - kErrorMargin) / 100, height);

    // Retrieve the checksum of the bitmap data from the pseudo printer and
    // compare it with the expected result.
    std::string bitmap_actual;
    EXPECT_TRUE(render_thread_.printer()->GetBitmapChecksum(0, &bitmap_actual));
    if (kTestPages[i].checksum)
      EXPECT_EQ(kTestPages[i].checksum, bitmap_actual);

    if (baseline) {
      // Save the source data and the bitmap data into temporary files to
      // create base-line results.
      FilePath source_path;
      file_util::CreateTemporaryFile(&source_path);
      render_thread_.printer()->SaveSource(0, source_path);

      FilePath bitmap_path;
      file_util::CreateTemporaryFile(&bitmap_path);
      render_thread_.printer()->SaveBitmap(0, bitmap_path);
    }
  }
}
#endif
