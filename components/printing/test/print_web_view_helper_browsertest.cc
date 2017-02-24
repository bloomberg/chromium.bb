// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/renderer/print_web_view_helper.h"

#include <stddef.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/printing/common/print_messages.h"
#include "components/printing/test/mock_printer.h"
#include "components/printing/test/print_mock_render_thread.h"
#include "components/printing/test/print_test_content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/render_view_test.h"
#include "ipc/ipc_listener.h"
#include "printing/features/features.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "base/files/file_util.h"
#include "printing/image.h"

using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebString;
#endif

namespace printing {

namespace {

// A simple web page.
const char kHelloWorldHTML[] = "<body><p>Hello World!</p></body>";

#if !defined(OS_CHROMEOS)
// A simple webpage with a button to print itself with.
const char kPrintOnUserAction[] =
    "<body>"
    "  <button id=\"print\" onclick=\"window.print();\">Hello World!</button>"
    "</body>";

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// HTML with 3 pages.
const char kMultipageHTML[] =
    "<html><head><style>"
    ".break { page-break-after: always; }"
    "</style></head>"
    "<body>"
    "<div class='break'>page1</div>"
    "<div class='break'>page2</div>"
    "<div>page3</div>"
    "</body></html>";

// A simple web page with print page size css.
const char kHTMLWithPageSizeCss[] =
    "<html><head><style>"
    "@media print {"
    "  @page {"
    "     size: 4in 4in;"
    "  }"
    "}"
    "</style></head>"
    "<body>Lorem Ipsum:"
    "</body></html>";

// A simple web page with print page layout css.
const char kHTMLWithLandscapePageCss[] =
    "<html><head><style>"
    "@media print {"
    "  @page {"
    "     size: landscape;"
    "  }"
    "}"
    "</style></head>"
    "<body>Lorem Ipsum:"
    "</body></html>";

// A longer web page.
const char kLongPageHTML[] =
    "<body><img src=\"\" width=10 height=10000 /></body>";

// A web page to simulate the print preview page.
const char kPrintPreviewHTML[] =
    "<body><p id=\"pdf-viewer\">Hello World!</p></body>";

void CreatePrintSettingsDictionary(base::DictionaryValue* dict) {
  dict->SetBoolean(kSettingLandscape, false);
  dict->SetBoolean(kSettingCollate, false);
  dict->SetInteger(kSettingColor, GRAY);
  dict->SetBoolean(kSettingPrintToPDF, true);
  dict->SetInteger(kSettingDuplexMode, SIMPLEX);
  dict->SetInteger(kSettingCopies, 1);
  dict->SetString(kSettingDeviceName, "dummy");
  dict->SetInteger(kPreviewUIID, 4);
  dict->SetInteger(kPreviewRequestID, 12345);
  dict->SetBoolean(kIsFirstRequest, true);
  dict->SetInteger(kSettingMarginsType, DEFAULT_MARGINS);
  dict->SetBoolean(kSettingPreviewModifiable, false);
  dict->SetBoolean(kSettingHeaderFooterEnabled, false);
  dict->SetBoolean(kSettingGenerateDraftData, true);
  dict->SetBoolean(kSettingShouldPrintBackgrounds, false);
  dict->SetBoolean(kSettingShouldPrintSelectionOnly, false);
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // !defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class DidPreviewPageListener : public IPC::Listener {
 public:
  explicit DidPreviewPageListener(base::RunLoop* run_loop)
      : run_loop_(run_loop) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    if (message.type() == PrintHostMsg_MetafileReadyForPrinting::ID ||
        message.type() == PrintHostMsg_PrintPreviewFailed::ID ||
        message.type() == PrintHostMsg_PrintPreviewCancelled::ID)
      run_loop_->Quit();
    return false;
  }

 private:
  base::RunLoop* const run_loop_;
  DISALLOW_COPY_AND_ASSIGN(DidPreviewPageListener);
};
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

}  // namespace

class PrintWebViewHelperTestBase : public content::RenderViewTest {
 public:
  PrintWebViewHelperTestBase() : print_render_thread_(nullptr) {}
  ~PrintWebViewHelperTestBase() override {}

 protected:
  // content::RenderViewTest:
  content::ContentRendererClient* CreateContentRendererClient() override {
    return new PrintTestContentRendererClient();
  }

  void SetUp() override {
    print_render_thread_ = new PrintMockRenderThread();
    render_thread_.reset(print_render_thread_);

    content::RenderViewTest::SetUp();
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
    // Do this before shutting down V8 in RenderViewTest::TearDown().
    // http://crbug.com/328552
    __lsan_do_leak_check();
#endif

    content::RenderViewTest::TearDown();
  }

  void PrintWithJavaScript() {
    ExecuteJavaScriptForTests("window.print();");
    ProcessPendingMessages();
  }

  // The renderer should be done calculating the number of rendered pages
  // according to the specified settings defined in the mock render thread.
  // Verify the page count is correct.
  void VerifyPageCount(int count) {
#if defined(OS_CHROMEOS)
    // The DidGetPrintedPagesCount message isn't sent on ChromeOS. Right now we
    // always print all pages, and there are checks to that effect built into
    // the print code.
#else
    const IPC::Message* page_cnt_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_DidGetPrintedPagesCount::ID);
    ASSERT_TRUE(page_cnt_msg);
    PrintHostMsg_DidGetPrintedPagesCount::Param post_page_count_param;
    PrintHostMsg_DidGetPrintedPagesCount::Read(page_cnt_msg,
                                               &post_page_count_param);
    EXPECT_EQ(count, std::get<1>(post_page_count_param));
#endif  // defined(OS_CHROMEOS)
  }

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // The renderer should be done calculating the number of rendered pages
  // according to the specified settings defined in the mock render thread.
  // Verify the page count is correct.
  void VerifyPreviewPageCount(int count) {
    const IPC::Message* page_cnt_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_DidGetPreviewPageCount::ID);
    ASSERT_TRUE(page_cnt_msg);
    PrintHostMsg_DidGetPreviewPageCount::Param post_page_count_param;
    PrintHostMsg_DidGetPreviewPageCount::Read(page_cnt_msg,
                                              &post_page_count_param);
    EXPECT_EQ(count, std::get<0>(post_page_count_param).page_count);
  }
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if defined(OS_WIN)
  // Verifies that the correct page size was returned.
  void VerifyPrintedPageSize(const gfx::Size& page_size) {
    const IPC::Message* print_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_DidPrintPage::ID);
    PrintHostMsg_DidPrintPage::Param post_did_print_page_param;
    PrintHostMsg_DidPrintPage::Read(print_msg, &post_did_print_page_param);
    gfx::Size page_size_received =
        std::get<0>(post_did_print_page_param).page_size;
    EXPECT_EQ(page_size, page_size_received);
  }
#endif

  // Verifies whether the pages printed or not.
  void VerifyPagesPrinted(bool printed) {
    const IPC::Message* print_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_DidPrintPage::ID);
    bool did_print_msg = !!print_msg;
    ASSERT_EQ(printed, did_print_msg);
    if (printed) {
      PrintHostMsg_DidPrintPage::Param post_did_print_page_param;
      PrintHostMsg_DidPrintPage::Read(print_msg, &post_did_print_page_param);
      EXPECT_EQ(0, std::get<0>(post_did_print_page_param).page_number);
    }
  }

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
  void OnPrintPages() {
    GetPrintWebViewHelper()->OnPrintPages();
    ProcessPendingMessages();
  }
#endif  // BUILDFLAG(ENABLE_BASIC_PRINTING)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void VerifyPreviewRequest(bool requested) {
    const IPC::Message* print_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_SetupScriptedPrintPreview::ID);
    bool did_print_msg = !!print_msg;
    ASSERT_EQ(requested, did_print_msg);
  }

  void OnPrintPreview(const base::DictionaryValue& dict) {
    PrintWebViewHelper* print_web_view_helper = GetPrintWebViewHelper();
    print_web_view_helper->OnInitiatePrintPreview(false);
    base::RunLoop run_loop;
    DidPreviewPageListener filter(&run_loop);
    render_thread_->sink().AddFilter(&filter);
    print_web_view_helper->OnPrintPreview(dict);
    run_loop.Run();
    render_thread_->sink().RemoveFilter(&filter);
  }
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
  void OnPrintForPrintPreview(const base::DictionaryValue& dict) {
    GetPrintWebViewHelper()->OnPrintForPrintPreview(dict);
    ProcessPendingMessages();
  }
#endif  // BUILDFLAG(ENABLE_BASIC_PRINTING)

  PrintWebViewHelper* GetPrintWebViewHelper() {
    return PrintWebViewHelper::Get(
        content::RenderFrame::FromWebFrame(GetMainFrame()));
  }

  // Naked pointer as ownership is with content::RenderViewTest::render_thread_.
  PrintMockRenderThread* print_render_thread_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintWebViewHelperTestBase);
};

// RenderViewTest-based tests crash on Android
// http://crbug.com/187500
#if defined(OS_ANDROID)
#define MAYBE_PrintWebViewHelperTest DISABLED_PrintWebViewHelperTest
#else
#define MAYBE_PrintWebViewHelperTest PrintWebViewHelperTest
#endif  // defined(OS_ANDROID)

class MAYBE_PrintWebViewHelperTest : public PrintWebViewHelperTestBase {
 public:
  MAYBE_PrintWebViewHelperTest() {}
  ~MAYBE_PrintWebViewHelperTest() override {}

  void SetUp() override { PrintWebViewHelperTestBase::SetUp(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MAYBE_PrintWebViewHelperTest);
};

// This tests only for platforms without print preview.
#if !BUILDFLAG(ENABLE_PRINT_PREVIEW)
// Tests that the renderer blocks window.print() calls if they occur too
// frequently.
TEST_F(MAYBE_PrintWebViewHelperTest, BlockScriptInitiatedPrinting) {
  // Pretend user will cancel printing.
  print_render_thread_->set_print_dialog_user_response(false);
  // Try to print with window.print() a few times.
  PrintWithJavaScript();
  PrintWithJavaScript();
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Pretend user will print. (but printing is blocked.)
  print_render_thread_->set_print_dialog_user_response(true);
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Unblock script initiated printing and verify printing works.
  GetPrintWebViewHelper()->scripting_throttler_.Reset();
  print_render_thread_->printer()->ResetPrinter();
  PrintWithJavaScript();
  VerifyPageCount(1);
  VerifyPagesPrinted(true);
}

// Tests that the renderer always allows window.print() calls if they are user
// initiated.
TEST_F(MAYBE_PrintWebViewHelperTest, AllowUserOriginatedPrinting) {
  // Pretend user will cancel printing.
  print_render_thread_->set_print_dialog_user_response(false);
  // Try to print with window.print() a few times.
  PrintWithJavaScript();
  PrintWithJavaScript();
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Pretend user will print. (but printing is blocked.)
  print_render_thread_->set_print_dialog_user_response(true);
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Try again as if user initiated, without resetting the print count.
  print_render_thread_->printer()->ResetPrinter();
  LoadHTML(kPrintOnUserAction);
  gfx::Size new_size(200, 100);
  Resize(new_size, false);

  gfx::Rect bounds = GetElementBounds("print");
  EXPECT_FALSE(bounds.IsEmpty());
  blink::WebMouseEvent mouse_event(blink::WebInputEvent::MouseDown,
                                   blink::WebInputEvent::NoModifiers,
                                   blink::WebInputEvent::TimeStampForTesting);
  mouse_event.button = blink::WebMouseEvent::Button::Left;
  mouse_event.x = bounds.CenterPoint().x();
  mouse_event.y = bounds.CenterPoint().y();
  mouse_event.clickCount = 1;
  SendWebMouseEvent(mouse_event);
  mouse_event.setType(blink::WebInputEvent::MouseUp);
  SendWebMouseEvent(mouse_event);
  ProcessPendingMessages();

  VerifyPageCount(1);
  VerifyPagesPrinted(true);
}

// Duplicate of OnPrintPagesTest only using javascript to print.
TEST_F(MAYBE_PrintWebViewHelperTest, PrintWithJavascript) {
  PrintWithJavaScript();

  VerifyPageCount(1);
  VerifyPagesPrinted(true);
}
#endif  // !BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
// Tests that printing pages work and sending and receiving messages through
// that channel all works.
TEST_F(MAYBE_PrintWebViewHelperTest, OnPrintPages) {
  LoadHTML(kHelloWorldHTML);
  OnPrintPages();

  VerifyPageCount(1);
  VerifyPagesPrinted(true);
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINTING)

#if defined(OS_MACOSX) && BUILDFLAG(ENABLE_BASIC_PRINTING)
// TODO(estade): I don't think this test is worth porting to Linux. We will have
// to rip out and replace most of the IPC code if we ever plan to improve
// printing, and the comment below by sverrir suggests that it doesn't do much
// for us anyway.
TEST_F(MAYBE_PrintWebViewHelperTest, PrintWithIframe) {
  // Document that populates an iframe.
  const char html[] =
      "<html><body>Lorem Ipsum:"
      "<iframe name=\"sub1\" id=\"sub1\"></iframe><script>"
      "  document.write(frames['sub1'].name);"
      "  frames['sub1'].document.write("
      "      '<p>Cras tempus ante eu felis semper luctus!</p>');"
      "  frames['sub1'].document.close();"
      "</script></body></html>";

  LoadHTML(html);

  // Find the frame and set it as the focused one.  This should mean that that
  // the printout should only contain the contents of that frame.
  auto* web_view = view_->GetWebView();
  WebFrame* sub1_frame = web_view->findFrameByName(WebString::fromUTF8("sub1"));
  ASSERT_TRUE(sub1_frame);
  web_view->setFocusedFrame(sub1_frame);
  ASSERT_NE(web_view->focusedFrame(), web_view->mainFrame());

  // Initiate printing.
  OnPrintPages();
  VerifyPagesPrinted(true);

  // Verify output through MockPrinter.
  const MockPrinter* printer(print_render_thread_->printer());
  ASSERT_EQ(1, printer->GetPrintedPages());
  const Image& image1(printer->GetPrintedPage(0)->image());

  // TODO(sverrir): Figure out a way to improve this test to actually print
  // only the content of the iframe.  Currently image1 will contain the full
  // page.
  EXPECT_NE(0, image1.size().width());
  EXPECT_NE(0, image1.size().height());
}
#endif  // OS_MACOSX && ENABLE_BASIC_PRINTING

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

#if defined(OS_MACOSX) && BUILDFLAG(ENABLE_BASIC_PRINTING)
const TestPageData kTestPages[] = {
    {
        "<html>"
        "<head>"
        "<meta"
        "  http-equiv=\"Content-Type\""
        "  content=\"text/html; charset=utf-8\"/>"
        "<title>Test 1</title>"
        "</head>"
        "<body style=\"background-color: white;\">"
        "<p style=\"font-family: arial;\">Hello World!</p>"
        "</body>",
        1,
        // Mac printing code compensates for the WebKit scale factor while
        // generating the metafile, so we expect smaller pages. (On non-Mac
        // platforms, this would be 675x900).
        600, 780, nullptr, nullptr,
    },
};
#endif  // defined(OS_MACOSX) && BUILDFLAG(ENABLE_BASIC_PRINTING)
}  // namespace

// TODO(estade): need to port MockPrinter to get this on Linux. This involves
// hooking up Cairo to read a pdf stream, or accessing the cairo surface in the
// metafile directly.
// Same for printing via PDF on Windows.
#if defined(OS_MACOSX) && BUILDFLAG(ENABLE_BASIC_PRINTING)
TEST_F(MAYBE_PrintWebViewHelperTest, PrintLayoutTest) {
  bool baseline = false;

  EXPECT_TRUE(print_render_thread_->printer());
  for (size_t i = 0; i < arraysize(kTestPages); ++i) {
    // Load an HTML page and print it.
    LoadHTML(kTestPages[i].page);
    OnPrintPages();
    VerifyPagesPrinted(true);

    // MockRenderThread::Send() just calls MockRenderThread::OnReceived().
    // So, all IPC messages sent in the above RenderView::OnPrintPages() call
    // has been handled by the MockPrinter object, i.e. this printing job
    // has been already finished.
    // So, we can start checking the output pages of this printing job.
    // Retrieve the number of pages actually printed.
    size_t pages = print_render_thread_->printer()->GetPrintedPages();
    EXPECT_EQ(kTestPages[i].printed_pages, pages);

    // Retrieve the width and height of the output page.
    int width = print_render_thread_->printer()->GetWidth(0);
    int height = print_render_thread_->printer()->GetHeight(0);

    // Check with margin for error.  This has been failing with a one pixel
    // offset on our buildbot.
    const int kErrorMargin = 5;  // 5%
    EXPECT_GT(kTestPages[i].width * (100 + kErrorMargin) / 100, width);
    EXPECT_LT(kTestPages[i].width * (100 - kErrorMargin) / 100, width);
    EXPECT_GT(kTestPages[i].height * (100 + kErrorMargin) / 100, height);
    EXPECT_LT(kTestPages[i].height * (100 - kErrorMargin) / 100, height);

    // Retrieve the checksum of the bitmap data from the pseudo printer and
    // compare it with the expected result.
    std::string bitmap_actual;
    EXPECT_TRUE(
        print_render_thread_->printer()->GetBitmapChecksum(0, &bitmap_actual));
    if (kTestPages[i].checksum)
      EXPECT_EQ(kTestPages[i].checksum, bitmap_actual);

    if (baseline) {
      // Save the source data and the bitmap data into temporary files to
      // create base-line results.
      base::FilePath source_path;
      base::CreateTemporaryFile(&source_path);
      print_render_thread_->printer()->SaveSource(0, source_path);

      base::FilePath bitmap_path;
      base::CreateTemporaryFile(&bitmap_path);
      print_render_thread_->printer()->SaveBitmap(0, bitmap_path);
    }
  }
}
#endif  // OS_MACOSX && ENABLE_BASIC_PRINTING

// These print preview tests do not work on Chrome OS yet.
#if !defined(OS_CHROMEOS)

// RenderViewTest-based tests crash on Android
// http://crbug.com/187500
#if defined(OS_ANDROID)
#define MAYBE_PrintWebViewHelperPreviewTest \
  DISABLED_PrintWebViewHelperPreviewTest
#else
#define MAYBE_PrintWebViewHelperPreviewTest PrintWebViewHelperPreviewTest
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class MAYBE_PrintWebViewHelperPreviewTest : public PrintWebViewHelperTestBase {
 public:
  MAYBE_PrintWebViewHelperPreviewTest() {}
  ~MAYBE_PrintWebViewHelperPreviewTest() override {}

 protected:
  void VerifyPrintPreviewCancelled(bool did_cancel) {
    bool print_preview_cancelled =
        !!render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_PrintPreviewCancelled::ID);
    EXPECT_EQ(did_cancel, print_preview_cancelled);
  }

  void VerifyPrintPreviewFailed(bool did_fail) {
    bool print_preview_failed =
        !!render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_PrintPreviewFailed::ID);
    EXPECT_EQ(did_fail, print_preview_failed);
  }

  void VerifyPrintPreviewGenerated(bool generated_preview) {
    const IPC::Message* preview_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_MetafileReadyForPrinting::ID);
    bool did_get_preview_msg = !!preview_msg;
    ASSERT_EQ(generated_preview, did_get_preview_msg);
    if (did_get_preview_msg) {
      PrintHostMsg_MetafileReadyForPrinting::Param preview_param;
      PrintHostMsg_MetafileReadyForPrinting::Read(preview_msg, &preview_param);
      EXPECT_NE(0, std::get<0>(preview_param).document_cookie);
      EXPECT_NE(0, std::get<0>(preview_param).expected_pages_count);
      EXPECT_NE(0U, std::get<0>(preview_param).data_size);
    }
  }

  void VerifyPrintFailed(bool did_fail) {
    bool print_failed = !!render_thread_->sink().GetUniqueMessageMatching(
        PrintHostMsg_PrintingFailed::ID);
    EXPECT_EQ(did_fail, print_failed);
  }

  void VerifyPrintPreviewInvalidPrinterSettings(bool settings_invalid) {
    bool print_preview_invalid_printer_settings =
        !!render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_PrintPreviewInvalidPrinterSettings::ID);
    EXPECT_EQ(settings_invalid, print_preview_invalid_printer_settings);
  }

  // |page_number| is 0-based.
  void VerifyDidPreviewPage(bool generate_draft_pages, int page_number) {
    bool msg_found = false;
    size_t msg_count = render_thread_->sink().message_count();
    for (size_t i = 0; i < msg_count; ++i) {
      const IPC::Message* msg = render_thread_->sink().GetMessageAt(i);
      if (msg->type() == PrintHostMsg_DidPreviewPage::ID) {
        PrintHostMsg_DidPreviewPage::Param page_param;
        PrintHostMsg_DidPreviewPage::Read(msg, &page_param);
        if (std::get<0>(page_param).page_number == page_number) {
          msg_found = true;
          if (generate_draft_pages)
            EXPECT_NE(0U, std::get<0>(page_param).data_size);
          else
            EXPECT_EQ(0U, std::get<0>(page_param).data_size);
          break;
        }
      }
    }
    ASSERT_EQ(generate_draft_pages, msg_found);
  }

  void VerifyDefaultPageLayout(int content_width,
                               int content_height,
                               int margin_top,
                               int margin_bottom,
                               int margin_left,
                               int margin_right,
                               bool page_has_print_css) {
    const IPC::Message* default_page_layout_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_DidGetDefaultPageLayout::ID);
    bool did_get_default_page_layout_msg = !!default_page_layout_msg;
    if (did_get_default_page_layout_msg) {
      PrintHostMsg_DidGetDefaultPageLayout::Param param;
      PrintHostMsg_DidGetDefaultPageLayout::Read(default_page_layout_msg,
                                                 &param);
      EXPECT_EQ(content_width, std::get<0>(param).content_width);
      EXPECT_EQ(content_height, std::get<0>(param).content_height);
      EXPECT_EQ(margin_top, std::get<0>(param).margin_top);
      EXPECT_EQ(margin_right, std::get<0>(param).margin_right);
      EXPECT_EQ(margin_left, std::get<0>(param).margin_left);
      EXPECT_EQ(margin_bottom, std::get<0>(param).margin_bottom);
      EXPECT_EQ(page_has_print_css, std::get<2>(param));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MAYBE_PrintWebViewHelperPreviewTest);
};

TEST_F(MAYBE_PrintWebViewHelperPreviewTest, BlockScriptInitiatedPrinting) {
  LoadHTML(kHelloWorldHTML);
  PrintWebViewHelper* print_web_view_helper = GetPrintWebViewHelper();
  print_web_view_helper->OnSetPrintingEnabled(false);
  PrintWithJavaScript();
  VerifyPreviewRequest(false);

  print_web_view_helper->OnSetPrintingEnabled(true);
  PrintWithJavaScript();
  VerifyPreviewRequest(true);
}

TEST_F(MAYBE_PrintWebViewHelperPreviewTest, PrintWithJavaScript) {
  LoadHTML(kPrintOnUserAction);
  gfx::Size new_size(200, 100);
  Resize(new_size, false);

  gfx::Rect bounds = GetElementBounds("print");
  EXPECT_FALSE(bounds.IsEmpty());
  blink::WebMouseEvent mouse_event(blink::WebInputEvent::MouseDown,
                                   blink::WebInputEvent::NoModifiers,
                                   blink::WebInputEvent::TimeStampForTesting);
  mouse_event.button = blink::WebMouseEvent::Button::Left;
  mouse_event.x = bounds.CenterPoint().x();
  mouse_event.y = bounds.CenterPoint().y();
  mouse_event.clickCount = 1;
  SendWebMouseEvent(mouse_event);
  mouse_event.setType(blink::WebInputEvent::MouseUp);
  SendWebMouseEvent(mouse_event);

  VerifyPreviewRequest(true);
}

// Tests that print preview work and sending and receiving messages through
// that channel all works.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, OnPrintPreview) {
  LoadHTML(kHelloWorldHTML);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());
  VerifyDefaultPageLayout(540, 720, 36, 36, 36, 36, false);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);
}

TEST_F(MAYBE_PrintWebViewHelperPreviewTest,
       PrintPreviewHTMLWithPageMarginsCss) {
  // A simple web page with print margins css.
  const char kHTMLWithPageMarginsCss[] =
      "<html><head><style>"
      "@media print {"
      "  @page {"
      "     margin: 3in 1in 2in 0.3in;"
      "  }"
      "}"
      "</style></head>"
      "<body>Lorem Ipsum:"
      "</body></html>";
  LoadHTML(kHTMLWithPageMarginsCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetBoolean(kSettingPrintToPDF, false);
  dict.SetInteger(kSettingMarginsType, DEFAULT_MARGINS);
  OnPrintPreview(dict);

  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());
  VerifyDefaultPageLayout(519, 432, 216, 144, 21, 72, false);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);
}

// Test to verify that print preview ignores print media css when non-default
// margin is selected.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest,
       NonDefaultMarginsSelectedIgnorePrintCss) {
  LoadHTML(kHTMLWithPageSizeCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetBoolean(kSettingPrintToPDF, false);
  dict.SetInteger(kSettingMarginsType, NO_MARGINS);
  OnPrintPreview(dict);

  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());
  VerifyDefaultPageLayout(612, 792, 0, 0, 0, 0, true);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);
}

// Test to verify that print preview honor print media size css when
// PRINT_TO_PDF is selected and doesn't fit to printer default paper size.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, PrintToPDFSelectedHonorPrintCss) {
  LoadHTML(kHTMLWithPageSizeCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetBoolean(kSettingPrintToPDF, true);
  dict.SetInteger(kSettingMarginsType, PRINTABLE_AREA_MARGINS);
  OnPrintPreview(dict);

  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());
  // Since PRINT_TO_PDF is selected, pdf page size is equal to print media page
  // size.
  VerifyDefaultPageLayout(252, 252, 18, 18, 18, 18, true);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
}

// Test to verify that print preview honor print margin css when PRINT_TO_PDF
// is selected and doesn't fit to printer default paper size.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest,
       PrintToPDFSelectedHonorPageMarginsCss) {
  // A simple web page with print margins css.
  const char kHTMLWithPageCss[] =
      "<html><head><style>"
      "@media print {"
      "  @page {"
      "     margin: 3in 1in 2in 0.3in;"
      "     size: 14in 14in;"
      "  }"
      "}"
      "</style></head>"
      "<body>Lorem Ipsum:"
      "</body></html>";
  LoadHTML(kHTMLWithPageCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetBoolean(kSettingPrintToPDF, true);
  dict.SetInteger(kSettingMarginsType, DEFAULT_MARGINS);
  OnPrintPreview(dict);

  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());
  // Since PRINT_TO_PDF is selected, pdf page size is equal to print media page
  // size.
  VerifyDefaultPageLayout(915, 648, 216, 144, 21, 72, true);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
}

// Test to verify that print preview workflow center the html page contents to
// fit the page size.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, PrintPreviewCenterToFitPage) {
  LoadHTML(kHTMLWithPageSizeCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetBoolean(kSettingPrintToPDF, false);
  dict.SetInteger(kSettingMarginsType, DEFAULT_MARGINS);
  OnPrintPreview(dict);

  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());
  VerifyDefaultPageLayout(216, 216, 288, 288, 198, 198, true);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
}

// Test to verify that print preview workflow scale the html page contents to
// fit the page size.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, PrintPreviewShrinkToFitPage) {
  // A simple web page with print margins css.
  const char kHTMLWithPageCss[] =
      "<html><head><style>"
      "@media print {"
      "  @page {"
      "     size: 15in 17in;"
      "  }"
      "}"
      "</style></head>"
      "<body>Lorem Ipsum:"
      "</body></html>";
  LoadHTML(kHTMLWithPageCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetBoolean(kSettingPrintToPDF, false);
  dict.SetInteger(kSettingMarginsType, DEFAULT_MARGINS);
  OnPrintPreview(dict);

  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());
  VerifyDefaultPageLayout(571, 652, 69, 71, 20, 21, true);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
}

// Test to verify that print preview workflow honor the orientation settings
// specified in css.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, PrintPreviewHonorsOrientationCss) {
  LoadHTML(kHTMLWithLandscapePageCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetBoolean(kSettingPrintToPDF, false);
  dict.SetInteger(kSettingMarginsType, NO_MARGINS);
  OnPrintPreview(dict);

  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());
  VerifyDefaultPageLayout(792, 612, 0, 0, 0, 0, true);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
}

// Test to verify that print preview workflow honors the orientation settings
// specified in css when PRINT_TO_PDF is selected.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest,
       PrintToPDFSelectedHonorOrientationCss) {
  LoadHTML(kHTMLWithLandscapePageCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetBoolean(kSettingPrintToPDF, true);
  dict.SetInteger(kSettingMarginsType, CUSTOM_MARGINS);
  OnPrintPreview(dict);

  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());
  VerifyDefaultPageLayout(748, 568, 21, 23, 21, 23, true);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
}

// Test to verify that complete metafile is generated for a subset of pages
// without creating draft pages.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, OnPrintPreviewForSelectedPages) {
  LoadHTML(kMultipageHTML);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);

  // Set a page range and update the dictionary to generate only the complete
  // metafile with the selected pages. Page numbers used in the dictionary
  // are 1-based.
  std::unique_ptr<base::DictionaryValue> page_range(
      new base::DictionaryValue());
  page_range->SetInteger(kSettingPageRangeFrom, 2);
  page_range->SetInteger(kSettingPageRangeTo, 3);

  base::ListValue* page_range_array = new base::ListValue();
  page_range_array->Append(std::move(page_range));

  dict.Set(kSettingPageRange, page_range_array);
  dict.SetBoolean(kSettingGenerateDraftData, false);

  OnPrintPreview(dict);

  VerifyDidPreviewPage(false, 0);
  VerifyDidPreviewPage(false, 1);
  VerifyDidPreviewPage(false, 2);
  VerifyPreviewPageCount(3);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);
}

// Test to verify that preview generated only for one page.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, OnPrintPreviewForSelectedText) {
  LoadHTML(kMultipageHTML);
  GetMainFrame()->selectRange(blink::WebRange(1, 3));

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetBoolean(kSettingShouldPrintSelectionOnly, true);

  OnPrintPreview(dict);

  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);
}

// Tests that print preview fails and receiving error messages through
// that channel all works.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, OnPrintPreviewFail) {
  LoadHTML(kHelloWorldHTML);

  // An empty dictionary should fail.
  base::DictionaryValue empty_dict;
  OnPrintPreview(empty_dict);

  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(true);
  VerifyPrintPreviewGenerated(false);
  VerifyPagesPrinted(false);
}

// Tests that cancelling print preview works.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, OnPrintPreviewCancel) {
  LoadHTML(kLongPageHTML);

  const int kCancelPage = 3;
  print_render_thread_->set_print_preview_cancel_page_number(kCancelPage);
  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  EXPECT_EQ(kCancelPage, print_render_thread_->print_preview_pages_remaining());
  VerifyPrintPreviewCancelled(true);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);
  VerifyPagesPrinted(false);
}

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
// Tests that printing from print preview works and sending and receiving
// messages through that channel all works.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, OnPrintForPrintPreview) {
  LoadHTML(kPrintPreviewHTML);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintForPrintPreview(dict);

  VerifyPrintFailed(false);
  VerifyPagesPrinted(true);
}

// Tests that when printing non-default scaling values, the page size returned
// by PrintWebViewHelper is still the real physical page size. See
// crbug.com/686384
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, OnPrintForPrintPreviewWithScaling) {
  LoadHTML(kPrintPreviewHTML);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);

  // Media size
  gfx::Size page_size_in = gfx::Size(240, 480);
  float device_microns_per_unit =
      (printing::kHundrethsMMPerInch * 10.0f) / printing::kDefaultPdfDpi;
  int height_microns =
      static_cast<int>(page_size_in.height() * device_microns_per_unit);
  int width_microns =
      static_cast<int>(page_size_in.width() * device_microns_per_unit);
  auto media_size = base::MakeUnique<base::DictionaryValue>();
  media_size->SetInteger(kSettingMediaSizeHeightMicrons, height_microns);
  media_size->SetInteger(kSettingMediaSizeWidthMicrons, width_microns);

  // Non default scaling value
  dict.SetInteger(kSettingScaleFactor, 80);
  dict.Set(kSettingMediaSize, media_size.release());

  OnPrintForPrintPreview(dict);

  VerifyPrintFailed(false);
  VerifyPagesPrinted(true);
#if defined(OS_WIN)
  VerifyPrintedPageSize(page_size_in);
#endif
}

// Tests that printing from print preview fails and receiving error messages
// through that channel all works.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest, OnPrintForPrintPreviewFail) {
  LoadHTML(kPrintPreviewHTML);

  // An empty dictionary should fail.
  base::DictionaryValue empty_dict;
  OnPrintForPrintPreview(empty_dict);

  VerifyPagesPrinted(false);
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINTING)

// Tests that when default printer has invalid printer settings, print preview
// receives error message.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest,
       OnPrintPreviewUsingInvalidPrinterSettings) {
  LoadHTML(kPrintPreviewHTML);

  // Set mock printer to provide invalid settings.
  print_render_thread_->printer()->UseInvalidSettings();

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  // We should have received invalid printer settings from |printer_|.
  VerifyPrintPreviewInvalidPrinterSettings(true);
  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());

  // It should receive the invalid printer settings message only.
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);
}

// Tests that when the selected printer has invalid page settings, print preview
// receives error message.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest,
       OnPrintPreviewUsingInvalidPageSize) {
  LoadHTML(kPrintPreviewHTML);

  print_render_thread_->printer()->UseInvalidPageSize();

  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  VerifyPrintPreviewInvalidPrinterSettings(true);
  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());

  // It should receive the invalid printer settings message only.
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);
}

// Tests that when the selected printer has invalid content settings, print
// preview receives error message.
TEST_F(MAYBE_PrintWebViewHelperPreviewTest,
       OnPrintPreviewUsingInvalidContentSize) {
  LoadHTML(kPrintPreviewHTML);

  print_render_thread_->printer()->UseInvalidContentSize();

  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  VerifyPrintPreviewInvalidPrinterSettings(true);
  EXPECT_EQ(0, print_render_thread_->print_preview_pages_remaining());

  // It should receive the invalid printer settings message only.
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);
}

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
TEST_F(MAYBE_PrintWebViewHelperPreviewTest,
       OnPrintForPrintPreviewUsingInvalidPrinterSettings) {
  LoadHTML(kPrintPreviewHTML);

  // Set mock printer to provide invalid settings.
  print_render_thread_->printer()->UseInvalidSettings();

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintForPrintPreview(dict);

  VerifyPrintFailed(true);
  VerifyPagesPrinted(false);
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINTING)
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#endif  // !defined(OS_CHROMEOS)

}  // namespace printing
