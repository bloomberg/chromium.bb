// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"
#include "chrome/common/print_messages.h"
#include "chrome/renderer/print_web_view_helper.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_view.h"
#include "printing/print_job_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "base/file_util.h"
#include "printing/image.h"

using WebKit::WebFrame;
using WebKit::WebString;
#endif

namespace {

// A simple web page.
const char kHelloWorldHTML[] = "<body><p>Hello World!</p></body>";

// A simple webpage that prints itself.
const char kPrintWithJSHTML[] =
    "<body>Hello<script>window.print()</script>World</body>";

// A longer web page.
const char kLongPageHTML[] =
    "<body><img src=\"\" width=10 height=10000 /></body>";

// A web page to simulate the print preview page.
const char kPrintPreviewHTML[] =
    "<body><p id=\"pdf-viewer\">Hello World!</p></body>";

void CreatePrintSettingsDictionary(DictionaryValue* dict) {
  dict->SetBoolean(printing::kSettingLandscape, false);
  dict->SetBoolean(printing::kSettingCollate, false);
  dict->SetInteger(printing::kSettingColor, printing::GRAY);
  dict->SetBoolean(printing::kSettingPrintToPDF, true);
  dict->SetInteger(printing::kSettingDuplexMode, printing::SIMPLEX);
  dict->SetInteger(printing::kSettingCopies, 1);
  dict->SetString(printing::kSettingDeviceName, "dummy");
  dict->SetString(printing::kPreviewUIAddr, "0xb33fbeef");
  dict->SetInteger(printing::kPreviewRequestID, 12345);
  dict->SetBoolean(printing::kIsFirstRequest, true);
  dict->SetInteger(printing::kSettingMarginsType, printing::DEFAULT_MARGINS);
  dict->SetBoolean(printing::kSettingPreviewModifiable, false);
  dict->SetBoolean(printing::kSettingHeaderFooterEnabled, false);
  dict->SetBoolean(printing::kSettingGenerateDraftData, true);
}

}  // namespace

class PrintWebViewHelperTestBase : public ChromeRenderViewTest {
 public:
  PrintWebViewHelperTestBase() {}
  ~PrintWebViewHelperTestBase() {}

 protected:
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
    EXPECT_EQ(count, post_page_count_param.b);
#endif  // defined(OS_CHROMEOS)
  }

  // Verifies whether the pages printed or not.
  void VerifyPagesPrinted(bool printed) {
#if defined(OS_CHROMEOS)
    bool did_print_msg = (render_thread_->sink().GetUniqueMessageMatching(
        PrintHostMsg_TempFileForPrintingWritten::ID) != NULL);
    ASSERT_EQ(printed, did_print_msg);
#else
    const IPC::Message* print_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_DidPrintPage::ID);
    bool did_print_msg = (NULL != print_msg);
    ASSERT_EQ(printed, did_print_msg);
    if (printed) {
      PrintHostMsg_DidPrintPage::Param post_did_print_page_param;
      PrintHostMsg_DidPrintPage::Read(print_msg, &post_did_print_page_param);
      EXPECT_EQ(0, post_did_print_page_param.a.page_number);
    }
#endif  // defined(OS_CHROMEOS)
  }

  void OnPrintPreview(const DictionaryValue& dict) {
    PrintWebViewHelper* print_web_view_helper = PrintWebViewHelper::Get(view_);
    print_web_view_helper->OnInitiatePrintPreview();
    print_web_view_helper->OnPrintPreview(dict);
  }

  void OnPrintForPrintPreview(const DictionaryValue& dict) {
    PrintWebViewHelper::Get(view_)->OnPrintForPrintPreview(dict);
  }

  DISALLOW_COPY_AND_ASSIGN(PrintWebViewHelperTestBase);
};

class PrintWebViewHelperTest : public PrintWebViewHelperTestBase {
 public:
  PrintWebViewHelperTest() {}
  virtual ~PrintWebViewHelperTest() {}

  virtual void SetUp() {
    ChromeRenderViewTest::SetUp();
  }

 protected:
  DISALLOW_COPY_AND_ASSIGN(PrintWebViewHelperTest);
};

// Tests that printing pages work and sending and receiving messages through
// that channel all works.
TEST_F(PrintWebViewHelperTest, OnPrintPages) {
  LoadHTML(kHelloWorldHTML);
  PrintWebViewHelper::Get(view_)->OnPrintPages();

  VerifyPageCount(1);
  VerifyPagesPrinted(true);
}

// Duplicate of OnPrintPagesTest only using javascript to print.
TEST_F(PrintWebViewHelperTest, PrintWithJavascript) {
  // HTML contains a call to window.print()
  LoadHTML(kPrintWithJSHTML);

  VerifyPageCount(1);
  VerifyPagesPrinted(true);
}

// Tests that the renderer blocks window.print() calls if they occur too
// frequently.
TEST_F(PrintWebViewHelperTest, BlockScriptInitiatedPrinting) {
  // Pretend user will cancel printing.
  chrome_render_thread_->set_print_dialog_user_response(false);
  // Try to print with window.print() a few times.
  LoadHTML(kPrintWithJSHTML);
  LoadHTML(kPrintWithJSHTML);
  LoadHTML(kPrintWithJSHTML);
  VerifyPagesPrinted(false);

  // Pretend user will print. (but printing is blocked.)
  chrome_render_thread_->set_print_dialog_user_response(true);
  LoadHTML(kPrintWithJSHTML);
  VerifyPagesPrinted(false);

  // Unblock script initiated printing and verify printing works.
  PrintWebViewHelper::Get(view_)->ResetScriptedPrintCount();
  chrome_render_thread_->printer()->ResetPrinter();
  LoadHTML(kPrintWithJSHTML);
  VerifyPageCount(1);
  VerifyPagesPrinted(true);
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// TODO(estade): I don't think this test is worth porting to Linux. We will have
// to rip out and replace most of the IPC code if we ever plan to improve
// printing, and the comment below by sverrir suggests that it doesn't do much
// for us anyway.
TEST_F(PrintWebViewHelperTest, PrintWithIframe) {
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
      view_->GetWebView()->findFrameByName(WebString::fromUTF8("sub1"));
  ASSERT_TRUE(sub1_frame);
  view_->GetWebView()->setFocusedFrame(sub1_frame);
  ASSERT_NE(view_->GetWebView()->focusedFrame(),
            view_->GetWebView()->mainFrame());

  // Initiate printing.
  PrintWebViewHelper::Get(view_)->OnPrintPages();

  // Verify output through MockPrinter.
  const MockPrinter* printer(chrome_render_thread_->printer());
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
TEST_F(PrintWebViewHelperTest, PrintLayoutTest) {
  bool baseline = false;

  EXPECT_TRUE(chrome_render_thread_->printer() != NULL);
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
    size_t pages = chrome_render_thread_->printer()->GetPrintedPages();
    EXPECT_EQ(kTestPages[i].printed_pages, pages);

    // Retrieve the width and height of the output page.
    int width = chrome_render_thread_->printer()->GetWidth(0);
    int height = chrome_render_thread_->printer()->GetHeight(0);

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
    EXPECT_TRUE(
        chrome_render_thread_->printer()->GetBitmapChecksum(0, &bitmap_actual));
    if (kTestPages[i].checksum)
      EXPECT_EQ(kTestPages[i].checksum, bitmap_actual);

    if (baseline) {
      // Save the source data and the bitmap data into temporary files to
      // create base-line results.
      FilePath source_path;
      file_util::CreateTemporaryFile(&source_path);
      chrome_render_thread_->printer()->SaveSource(0, source_path);

      FilePath bitmap_path;
      file_util::CreateTemporaryFile(&bitmap_path);
      chrome_render_thread_->printer()->SaveBitmap(0, bitmap_path);
    }
  }
}
#endif

// These print preview tests do not work on Chrome OS yet.
#if !defined(OS_CHROMEOS)
class PrintWebViewHelperPreviewTest : public PrintWebViewHelperTestBase {
 public:
  PrintWebViewHelperPreviewTest() {}
  virtual ~PrintWebViewHelperPreviewTest() {}

  virtual void SetUp() {
    // Append the print preview switch before creating the PrintWebViewHelper.
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnablePrintPreview);

    ChromeRenderViewTest::SetUp();
  }

 protected:
  void VerifyPrintPreviewCancelled(bool did_cancel) {
    bool print_preview_cancelled =
        (render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_PrintPreviewCancelled::ID) != NULL);
    EXPECT_EQ(did_cancel, print_preview_cancelled);
  }

  void VerifyPrintPreviewFailed(bool did_fail) {
    bool print_preview_failed =
        (render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_PrintPreviewFailed::ID) != NULL);
    EXPECT_EQ(did_fail, print_preview_failed);
  }

  void VerifyPrintPreviewGenerated(bool generated_preview) {
    const IPC::Message* preview_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_MetafileReadyForPrinting::ID);
    bool did_get_preview_msg = (NULL != preview_msg);
    ASSERT_EQ(generated_preview, did_get_preview_msg);
    if (did_get_preview_msg) {
      PrintHostMsg_MetafileReadyForPrinting::Param preview_param;
      PrintHostMsg_MetafileReadyForPrinting::Read(preview_msg, &preview_param);
      EXPECT_NE(0, preview_param.a.document_cookie);
      EXPECT_NE(0, preview_param.a.expected_pages_count);
      EXPECT_NE(0U, preview_param.a.data_size);
    }
  }

  void VerifyPrintFailed(bool did_fail) {
    bool print_failed = (render_thread_->sink().GetUniqueMessageMatching(
        PrintHostMsg_PrintingFailed::ID) != NULL);
    EXPECT_EQ(did_fail, print_failed);
  }

  void VerifyPrintPreviewInvalidPrinterSettings(bool settings_invalid) {
    bool print_preview_invalid_printer_settings =
        (render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_PrintPreviewInvalidPrinterSettings::ID) != NULL);
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
        if (page_param.a.page_number == page_number) {
          msg_found = true;
          if (generate_draft_pages)
            EXPECT_NE(0U, page_param.a.data_size);
          else
            EXPECT_EQ(0U, page_param.a.data_size);
          break;
        }
      }
    }
    ASSERT_EQ(generate_draft_pages, msg_found);
  }

  DISALLOW_COPY_AND_ASSIGN(PrintWebViewHelperPreviewTest);
};

// Tests that print preview work and sending and receiving messages through
// that channel all works.
TEST_F(PrintWebViewHelperPreviewTest, OnPrintPreview) {
  LoadHTML(kHelloWorldHTML);

  // Fill in some dummy values.
  DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  EXPECT_EQ(0, chrome_render_thread_->print_preview_pages_remaining());
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);
}

// Test to verify that complete metafile is generated for a subset of pages
// without creating draft pages.
TEST_F(PrintWebViewHelperPreviewTest, OnPrintPreviewForSelectedPages) {
  LoadHTML(kHelloWorldHTML);

  // Fill in some dummy values.
  DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);

  // Set a page range and update the dictionary to generate only the complete
  // metafile with the selected pages. Page numbers used in the dictionary
  // are 1-based.
  DictionaryValue* page_range = new DictionaryValue();
  page_range->SetInteger(printing::kSettingPageRangeFrom, 1);
  page_range->SetInteger(printing::kSettingPageRangeTo, 1);

  ListValue* page_range_array = new ListValue();
  page_range_array->Append(page_range);

  dict.Set(printing::kSettingPageRange, page_range_array);
  dict.SetBoolean(printing::kSettingGenerateDraftData, false);

  OnPrintPreview(dict);

  // Verify that we did not create the draft metafile for the first page.
  VerifyDidPreviewPage(false, 0);

  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);
}

// Tests that print preview fails and receiving error messages through
// that channel all works.
TEST_F(PrintWebViewHelperPreviewTest, OnPrintPreviewFail) {
  LoadHTML(kHelloWorldHTML);

  // An empty dictionary should fail.
  DictionaryValue empty_dict;
  OnPrintPreview(empty_dict);

  EXPECT_EQ(0, chrome_render_thread_->print_preview_pages_remaining());
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(true);
  VerifyPrintPreviewGenerated(false);
  VerifyPagesPrinted(false);
}

// Tests that cancelling print preview works.
TEST_F(PrintWebViewHelperPreviewTest, OnPrintPreviewCancel) {
  LoadHTML(kLongPageHTML);

  const int kCancelPage = 3;
  chrome_render_thread_->set_print_preview_cancel_page_number(kCancelPage);
  // Fill in some dummy values.
  DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  EXPECT_EQ(kCancelPage,
            chrome_render_thread_->print_preview_pages_remaining());
  VerifyPrintPreviewCancelled(true);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);
  VerifyPagesPrinted(false);
}

// Tests that printing from print preview works and sending and receiving
// messages through that channel all works.
TEST_F(PrintWebViewHelperPreviewTest, OnPrintForPrintPreview) {
  LoadHTML(kPrintPreviewHTML);

  // Fill in some dummy values.
  DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintForPrintPreview(dict);

  VerifyPrintFailed(false);
  VerifyPagesPrinted(true);
}

// Tests that printing from print preview fails and receiving error messages
// through that channel all works.
TEST_F(PrintWebViewHelperPreviewTest, OnPrintForPrintPreviewFail) {
  LoadHTML(kPrintPreviewHTML);

  // An empty dictionary should fail.
  DictionaryValue empty_dict;
  OnPrintForPrintPreview(empty_dict);

  VerifyPagesPrinted(false);
}

// Tests that when default printer has invalid printer settings, print preview
// receives error message.
TEST_F(PrintWebViewHelperPreviewTest,
       OnPrintPreviewUsingInvalidPrinterSettings) {
  LoadHTML(kPrintPreviewHTML);

  // Set mock printer to provide invalid settings.
  chrome_render_thread_->printer()->UseInvalidSettings();

  // Fill in some dummy values.
  DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  // We should have received invalid printer settings from |printer_|.
  VerifyPrintPreviewInvalidPrinterSettings(true);
  EXPECT_EQ(0, chrome_render_thread_->print_preview_pages_remaining());

  // It should receive the invalid printer settings message only.
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);
}

TEST_F(PrintWebViewHelperPreviewTest,
       OnPrintForPrintPreviewUsingInvalidPrinterSettings) {
  LoadHTML(kPrintPreviewHTML);

  // Set mock printer to provide invalid settings.
  chrome_render_thread_->printer()->UseInvalidSettings();

  // Fill in some dummy values.
  DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintForPrintPreview(dict);

  VerifyPrintFailed(true);
  VerifyPagesPrinted(false);
}

#endif  // !defined(OS_CHROMEOS)
