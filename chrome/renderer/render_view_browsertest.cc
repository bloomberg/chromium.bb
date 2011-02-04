// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"

#include "base/file_util.h"
#include "base/shared_memory.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/autofill/autofill_agent.h"
#include "chrome/renderer/print_web_view_helper.h"
#include "chrome/test/render_view_test.h"
#include "gfx/codec/jpeg_codec.h"
#include "net/base/net_errors.h"
#include "printing/image.h"
#include "printing/native_metafile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/web_io_operators.h"

using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebString;
using WebKit::WebTextDirection;
using WebKit::WebURLError;
using webkit_glue::FormData;
using webkit_glue::FormField;

namespace {

// TODO(isherman): Pull this as a named constant from WebKit
const int kDefaultMaxLength = 0x80000;

}  // namespace

// Test that we get form state change notifications when input fields change.
TEST_F(RenderViewTest, OnNavStateChanged) {
  // Don't want any delay for form state sync changes. This will still post a
  // message so updates will get coalesced, but as soon as we spin the message
  // loop, it will generate an update.
  view_->set_send_content_state_immediately(true);

  LoadHTML("<input type=\"text\" id=\"elt_text\"></input>");

  // We should NOT have gotten a form state change notification yet.
  EXPECT_FALSE(render_thread_.sink().GetFirstMessageMatching(
      ViewHostMsg_UpdateState::ID));
  render_thread_.sink().ClearMessages();

  // Change the value of the input. We should have gotten an update state
  // notification. We need to spin the message loop to catch this update.
  ExecuteJavaScript("document.getElementById('elt_text').value = 'foo';");
  ProcessPendingMessages();
  EXPECT_TRUE(render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID));
}

// Test that we get the correct UpdateState message when we go back twice
// quickly without committing.  Regression test for http://crbug.com/58082.
TEST_F(RenderViewTest, LastCommittedUpdateState) {
  // Load page A.
  LoadHTML("<div>Page A</div>");

  // Load page B, which will trigger an UpdateState message for page A.
  LoadHTML("<div>Page B</div>");

  // Check for a valid UpdateState message for page A.
  const IPC::Message* msg_A = render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_A);
  int page_id_A;
  std::string state_A;
  ViewHostMsg_UpdateState::Read(msg_A, &page_id_A, &state_A);
  EXPECT_EQ(1, page_id_A);
  render_thread_.sink().ClearMessages();

  // Load page C, which will trigger an UpdateState message for page B.
  LoadHTML("<div>Page C</div>");

  // Check for a valid UpdateState for page B.
  const IPC::Message* msg_B = render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_B);
  int page_id_B;
  std::string state_B;
  ViewHostMsg_UpdateState::Read(msg_B, &page_id_B, &state_B);
  EXPECT_EQ(2, page_id_B);
  EXPECT_NE(state_A, state_B);
  render_thread_.sink().ClearMessages();

  // Load page D, which will trigger an UpdateState message for page C.
  LoadHTML("<div>Page D</div>");

  // Check for a valid UpdateState for page C.
  const IPC::Message* msg_C = render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_C);
  int page_id_C;
  std::string state_C;
  ViewHostMsg_UpdateState::Read(msg_C, &page_id_C, &state_C);
  EXPECT_EQ(3, page_id_C);
  EXPECT_NE(state_B, state_C);
  render_thread_.sink().ClearMessages();

  // Go back to C and commit, preparing for our real test.
  ViewMsg_Navigate_Params params_C;
  params_C.transition = PageTransition::FORWARD_BACK;
  params_C.page_id = 3;
  params_C.state = state_C;
  view_->OnNavigate(params_C);
  ProcessPendingMessages();
  render_thread_.sink().ClearMessages();

  // Go back twice quickly, such that page B does not have a chance to commit.
  // This leads to two changes to the back/forward list but only one change to
  // the RenderView's page ID.

  // Back to page B (page_id 2), without committing.
  ViewMsg_Navigate_Params params_B;
  params_B.transition = PageTransition::FORWARD_BACK;
  params_B.page_id = 2;
  params_B.state = state_B;
  view_->OnNavigate(params_B);

  // Back to page A (page_id 1) and commit.
  ViewMsg_Navigate_Params params;
  params.transition = PageTransition::FORWARD_BACK;
  params.page_id = 1;
  params.state = state_A;
  view_->OnNavigate(params);
  ProcessPendingMessages();

  // Now ensure that the UpdateState message we receive is consistent
  // and represents page C in both page_id and state.
  const IPC::Message* msg = render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg);
  int page_id;
  std::string state;
  ViewHostMsg_UpdateState::Read(msg, &page_id, &state);
  EXPECT_EQ(page_id_C, page_id);
  EXPECT_NE(state_A, state);
  EXPECT_NE(state_B, state);
  EXPECT_EQ(state_C, state);
}

// Test that our IME backend sends a notification message when the input focus
// changes.
TEST_F(RenderViewTest, OnImeStateChanged) {
  // Enable our IME backend code.
  view_->OnSetInputMethodActive(true);

  // Load an HTML page consisting of two input fields.
  view_->set_send_content_state_immediately(true);
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<input id=\"test1\" type=\"text\"></input>"
           "<input id=\"test2\" type=\"password\"></input>"
           "</body>"
           "</html>");
  render_thread_.sink().ClearMessages();

  const int kRepeatCount = 10;
  for (int i = 0; i < kRepeatCount; i++) {
    // Move the input focus to the first <input> element, where we should
    // activate IMEs.
    ExecuteJavaScript("document.getElementById('test1').focus();");
    ProcessPendingMessages();
    render_thread_.sink().ClearMessages();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to activate IMEs.
    view_->UpdateInputMethod();
    const IPC::Message* msg = render_thread_.sink().GetMessageAt(0);
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(ViewHostMsg_ImeUpdateTextInputState::ID, msg->type());
    ViewHostMsg_ImeUpdateTextInputState::Param params;
    ViewHostMsg_ImeUpdateTextInputState::Read(msg, &params);
    EXPECT_EQ(params.a, WebKit::WebTextInputTypeText);
    EXPECT_TRUE(params.b.x() > 0 && params.b.y() > 0);

    // Move the input focus to the second <input> element, where we should
    // de-activate IMEs.
    ExecuteJavaScript("document.getElementById('test2').focus();");
    ProcessPendingMessages();
    render_thread_.sink().ClearMessages();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to de-activate IMEs.
    view_->UpdateInputMethod();
    msg = render_thread_.sink().GetMessageAt(0);
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(ViewHostMsg_ImeUpdateTextInputState::ID, msg->type());
    ViewHostMsg_ImeUpdateTextInputState::Read(msg, &params);
    EXPECT_EQ(params.a, WebKit::WebTextInputTypePassword);
  }
}

// Test that our IME backend can compose CJK words.
// Our IME front-end sends many platform-independent messages to the IME backend
// while it composes CJK words. This test sends the minimal messages captured
// on my local environment directly to the IME backend to verify if the backend
// can compose CJK words without any problems.
// This test uses an array of command sets because an IME composotion does not
// only depends on IME events, but also depends on window events, e.g. moving
// the window focus while composing a CJK text. To handle such complicated
// cases, this test should not only call IME-related functions in the
// RenderWidget class, but also call some RenderWidget members, e.g.
// ExecuteJavaScript(), RenderWidget::OnSetFocus(), etc.
TEST_F(RenderViewTest, ImeComposition) {
  enum ImeCommand {
    IME_INITIALIZE,
    IME_SETINPUTMODE,
    IME_SETFOCUS,
    IME_SETCOMPOSITION,
    IME_CONFIRMCOMPOSITION,
    IME_CANCELCOMPOSITION
  };
  struct ImeMessage {
    ImeCommand command;
    bool enable;
    int selection_start;
    int selection_end;
    const wchar_t* ime_string;
    const wchar_t* result;
  };
  static const ImeMessage kImeMessages[] = {
    // Scenario 1: input a Chinese word with Microsoft IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 1, 1, L"n", L"n"},
    {IME_SETCOMPOSITION, false, 2, 2, L"ni", L"ni"},
    {IME_SETCOMPOSITION, false, 3, 3, L"nih", L"nih"},
    {IME_SETCOMPOSITION, false, 4, 4, L"niha", L"niha"},
    {IME_SETCOMPOSITION, false, 5, 5, L"nihao", L"nihao"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"\x4F60\x597D", L"\x4F60\x597D"},
    // Scenario 2: input a Japanese word with Microsoft IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xFF4B", L"\xFF4B"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\x304B", L"\x304B"},
    {IME_SETCOMPOSITION, false, 0, 2, L"\x304B\xFF4E", L"\x304B\xFF4E"},
    {IME_SETCOMPOSITION, false, 0, 3, L"\x304B\x3093\xFF4A",
     L"\x304B\x3093\xFF4A"},
    {IME_SETCOMPOSITION, false, 0, 3, L"\x304B\x3093\x3058",
     L"\x304B\x3093\x3058"},
    {IME_SETCOMPOSITION, false, 0, 2, L"\x611F\x3058", L"\x611F\x3058"},
    {IME_SETCOMPOSITION, false, 0, 2, L"\x6F22\x5B57", L"\x6F22\x5B57"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"", L"\x6F22\x5B57"},
    {IME_CANCELCOMPOSITION, false, -1, -1, L"", L"\x6F22\x5B57"},
    // Scenario 3: input a Korean word with Microsot IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 0, 1, L"\x3147", L"\x3147"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xC544", L"\xC544"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xC548", L"\xC548"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"", L"\xC548"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\x3134", L"\xC548\x3134"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xB140", L"\xC548\xB140"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xB155", L"\xC548\xB155"},
    {IME_CANCELCOMPOSITION, false, -1, -1, L"", L"\xC548"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xB155", L"\xC548\xB155"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"", L"\xC548\xB155"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kImeMessages); i++) {
    const ImeMessage* ime_message = &kImeMessages[i];
    switch (ime_message->command) {
      case IME_INITIALIZE:
        // Load an HTML page consisting of a content-editable <div> element,
        // and move the input focus to the <div> element, where we can use
        // IMEs.
        view_->OnSetInputMethodActive(ime_message->enable);
        view_->set_send_content_state_immediately(true);
        LoadHTML("<html>"
                "<head>"
                "</head>"
                "<body>"
                "<div id=\"test1\" contenteditable=\"true\"></div>"
                "</body>"
                "</html>");
        ExecuteJavaScript("document.getElementById('test1').focus();");
        break;

      case IME_SETINPUTMODE:
        // Activate (or deactivate) our IME back-end.
        view_->OnSetInputMethodActive(ime_message->enable);
        break;

      case IME_SETFOCUS:
        // Update the window focus.
        view_->OnSetFocus(ime_message->enable);
        break;

      case IME_SETCOMPOSITION:
        view_->OnImeSetComposition(
            WideToUTF16Hack(ime_message->ime_string),
            std::vector<WebKit::WebCompositionUnderline>(),
            ime_message->selection_start,
            ime_message->selection_end);
        break;

      case IME_CONFIRMCOMPOSITION:
        view_->OnImeConfirmComposition(
            WideToUTF16Hack(ime_message->ime_string));
        break;

      case IME_CANCELCOMPOSITION:
        view_->OnImeSetComposition(string16(),
                                std::vector<WebKit::WebCompositionUnderline>(),
                                0, 0);
        break;
    }

    // Update the status of our IME back-end.
    // TODO(hbono): we should verify messages to be sent from the back-end.
    view_->UpdateInputMethod();
    ProcessPendingMessages();
    render_thread_.sink().ClearMessages();

    if (ime_message->result) {
      // Retrieve the content of this page and compare it with the expected
      // result.
      const int kMaxOutputCharacters = 128;
      std::wstring output = UTF16ToWideHack(
          GetMainFrame()->contentAsText(kMaxOutputCharacters));
      EXPECT_EQ(output, ime_message->result);
    }
  }
}

// Test that the RenderView::OnSetTextDirection() function can change the text
// direction of the selected input element.
TEST_F(RenderViewTest, OnSetTextDirection) {
  // Load an HTML page consisting of a <textarea> element and a <div> element.
  // This test changes the text direction of the <textarea> element, and
  // writes the values of its 'dir' attribute and its 'direction' property to
  // verify that the text direction is changed.
  view_->set_send_content_state_immediately(true);
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<textarea id=\"test\"></textarea>"
           "<div id=\"result\" contenteditable=\"true\"></div>"
           "</body>"
           "</html>");
  render_thread_.sink().ClearMessages();

  static const struct {
    WebTextDirection direction;
    const wchar_t* expected_result;
  } kTextDirection[] = {
    { WebKit::WebTextDirectionRightToLeft, L"\x000A" L"rtl,rtl" },
    { WebKit::WebTextDirectionLeftToRight, L"\x000A" L"ltr,ltr" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTextDirection); ++i) {
    // Set the text direction of the <textarea> element.
    ExecuteJavaScript("document.getElementById('test').focus();");
    view_->OnSetTextDirection(kTextDirection[i].direction);

    // Write the values of its DOM 'dir' attribute and its CSS 'direction'
    // property to the <div> element.
    ExecuteJavaScript("var result = document.getElementById('result');"
                      "var node = document.getElementById('test');"
                      "var style = getComputedStyle(node, null);"
                      "result.innerText ="
                      "    node.getAttribute('dir') + ',' +"
                      "    style.getPropertyValue('direction');");

    // Copy the document content to std::wstring and compare with the
    // expected result.
    const int kMaxOutputCharacters = 16;
    std::wstring output = UTF16ToWideHack(
        GetMainFrame()->contentAsText(kMaxOutputCharacters));
    EXPECT_EQ(output, kTextDirection[i].expected_result);
  }
}

// Tests that printing pages work and sending and receiving messages through
// that channel all works.
TEST_F(RenderViewTest, OnPrintPages) {
  // Lets simulate a print pages with Hello world.
  LoadHTML("<body><p>Hello World!</p></body>");
  view_->OnPrintPages();

  VerifyPageCount(1);
  VerifyPagesPrinted();
}

// Duplicate of OnPrintPagesTest only using javascript to print.
TEST_F(RenderViewTest, PrintWithJavascript) {
  // HTML contains a call to window.print()
  LoadHTML("<body>Hello<script>window.print()</script>World</body>");

  VerifyPageCount(1);
  VerifyPagesPrinted();
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
  view_->OnPrintPages();

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
  1, 612, 792,
#else
  1, 764, 972,
#endif
  NULL,
  NULL,
  },
};
}  // namespace

// Failing on windows.  http://crbug.com/56246
#if defined(OS_WIN)
#define MAYBE_PrintLayoutTest DISABLED_PrintLayoutTest
#else
#define MAYBE_PrintLayoutTest PrintLayoutTest
#endif

// TODO(estade): need to port MockPrinter to get this on Linux. This involves
// hooking up Cairo to read a pdf stream, or accessing the cairo surface in the
// metafile directly.
#if defined(OS_WIN) || defined(OS_MACOSX)
TEST_F(RenderViewTest, MAYBE_PrintLayoutTest) {
  bool baseline = false;

  EXPECT_TRUE(render_thread_.printer() != NULL);
  for (size_t i = 0; i < arraysize(kTestPages); ++i) {
    // Load an HTML page and print it.
    LoadHTML(kTestPages[i].page);
    view_->OnPrintPages();

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

// Test that we can receive correct DOM events when we send input events
// through the RenderWidget::OnHandleInputEvent() function.
TEST_F(RenderViewTest, OnHandleKeyboardEvent) {
#if defined(OS_WIN) || defined(OS_LINUX)
  // Load an HTML page consisting of one <input> element and three
  // contentediable <div> elements.
  // The <input> element is used for sending keyboard events, and the <div>
  // elements are used for writing DOM events in the following format:
  //   "<keyCode>,<shiftKey>,<controlKey>,<altKey>".
  // TODO(hbono): <http://crbug.com/2215> Our WebKit port set |ev.metaKey| to
  // true when pressing an alt key, i.e. the |ev.metaKey| value is not
  // trustworthy. We will check the |ev.metaKey| value when this issue is fixed.
  view_->set_send_content_state_immediately(true);
  LoadHTML("<html>"
           "<head>"
           "<title></title>"
           "<script type='text/javascript' language='javascript'>"
           "function OnKeyEvent(ev) {"
           "  var result = document.getElementById(ev.type);"
           "  result.innerText ="
           "      (ev.which || ev.keyCode) + ',' +"
           "      ev.shiftKey + ',' +"
           "      ev.ctrlKey + ',' +"
           "      ev.altKey;"
           "  return true;"
           "}"
           "</script>"
           "</head>"
           "<body>"
           "<input id='test' type='text'"
           "    onkeydown='return OnKeyEvent(event);'"
           "    onkeypress='return OnKeyEvent(event);'"
           "    onkeyup='return OnKeyEvent(event);'>"
           "</input>"
           "<div id='keydown' contenteditable='true'>"
           "</div>"
           "<div id='keypress' contenteditable='true'>"
           "</div>"
           "<div id='keyup' contenteditable='true'>"
           "</div>"
           "</body>"
           "</html>");
  ExecuteJavaScript("document.getElementById('test').focus();");
  render_thread_.sink().ClearMessages();

  static const MockKeyboard::Layout kLayouts[] = {
#if defined(OS_WIN)
    // Since we ignore the mock keyboard layout on Linux and instead just use
    // the screen's keyboard layout, these trivially pass. They are commented
    // out to avoid the illusion that they work.
    MockKeyboard::LAYOUT_ARABIC,
    MockKeyboard::LAYOUT_CANADIAN_FRENCH,
    MockKeyboard::LAYOUT_FRENCH,
    MockKeyboard::LAYOUT_HEBREW,
    MockKeyboard::LAYOUT_RUSSIAN,
#endif
    MockKeyboard::LAYOUT_UNITED_STATES,
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kLayouts); ++i) {
    // For each key code, we send three keyboard events.
    //  * we press only the key;
    //  * we press the key and a left-shift key, and;
    //  * we press the key and a right-alt (AltGr) key.
    // For each modifiers, we need a string used for formatting its expected
    // result. (See the above comment for its format.)
    static const struct {
      MockKeyboard::Modifiers modifiers;
      const char* expected_result;
    } kModifierData[] = {
      {MockKeyboard::NONE,       "false,false,false"},
      {MockKeyboard::LEFT_SHIFT, "true,false,false"},
#if defined(OS_WIN)
      {MockKeyboard::RIGHT_ALT,  "false,false,true"},
#endif
    };

    MockKeyboard::Layout layout = kLayouts[i];
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(kModifierData); ++j) {
      // Virtual key codes used for this test.
      static const int kKeyCodes[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
        'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
        'W', 'X', 'Y', 'Z',
        ui::VKEY_OEM_1,
        ui::VKEY_OEM_PLUS,
        ui::VKEY_OEM_COMMA,
        ui::VKEY_OEM_MINUS,
        ui::VKEY_OEM_PERIOD,
        ui::VKEY_OEM_2,
        ui::VKEY_OEM_3,
        ui::VKEY_OEM_4,
        ui::VKEY_OEM_5,
        ui::VKEY_OEM_6,
        ui::VKEY_OEM_7,
#if defined(OS_WIN)
        // Not sure how to handle this key on Linux.
        ui::VKEY_OEM_8,
#endif
      };

      MockKeyboard::Modifiers modifiers = kModifierData[j].modifiers;
      for (size_t k = 0; k < ARRAYSIZE_UNSAFE(kKeyCodes); ++k) {
        // Send a keyboard event to the RenderView object.
        // We should test a keyboard event only when the given keyboard-layout
        // driver is installed in a PC and the driver can assign a Unicode
        // charcter for the given tuple (key-code and modifiers).
        int key_code = kKeyCodes[k];
        std::wstring char_code;
        if (SendKeyEvent(layout, key_code, modifiers, &char_code) < 0)
          continue;

        // Create an expected result from the virtual-key code, the character
        // code, and the modifier-key status.
        // We format a string that emulates a DOM-event string produced hy
        // our JavaScript function. (See the above comment for the format.)
        static char expected_result[1024];
        expected_result[0] = 0;
        base::snprintf(&expected_result[0],
                       sizeof(expected_result),
                       "\n"       // texts in the <input> element
                       "%d,%s\n"  // texts in the first <div> element
                       "%d,%s\n"  // texts in the second <div> element
                       "%d,%s",   // texts in the third <div> element
                       key_code, kModifierData[j].expected_result,
                       static_cast<int>(char_code[0]),
                       kModifierData[j].expected_result,
                       key_code, kModifierData[j].expected_result);

        // Retrieve the text in the test page and compare it with the expected
        // text created from a virtual-key code, a character code, and the
        // modifier-key status.
        const int kMaxOutputCharacters = 1024;
        std::string output = UTF16ToUTF8(
            GetMainFrame()->contentAsText(kMaxOutputCharacters));
        EXPECT_EQ(expected_result, output);
      }
    }
  }
#else
  NOTIMPLEMENTED();
#endif
}

// Test that our EditorClientImpl class can insert characters when we send
// keyboard events through the RenderWidget::OnHandleInputEvent() function.
// This test is for preventing regressions caused only when we use non-US
// keyboards, such as Issue 10846.
TEST_F(RenderViewTest, InsertCharacters) {
#if defined(OS_WIN) || defined(OS_LINUX)
  static const struct {
    MockKeyboard::Layout layout;
    const wchar_t* expected_result;
  } kLayouts[] = {
#if 0
    // Disabled these keyboard layouts because buildbots do not have their
    // keyboard-layout drivers installed.
    {MockKeyboard::LAYOUT_ARABIC,
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x0634\x0624\x064a\x062b\x0628\x0644"
     L"\x0627\x0647\x062a\x0646\x0645\x0629\x0649\x062e"
     L"\x062d\x0636\x0642\x0633\x0641\x0639\x0631\x0635"
     L"\x0621\x063a\x0626\x0643\x003d\x0648\x002d\x0632"
     L"\x0638\x0630\x062c\x005c\x062f\x0637\x0028\x0021"
     L"\x0040\x0023\x0024\x0025\x005e\x0026\x002a\x0029"
     L"\x0650\x007d\x005d\x064f\x005b\x0623\x00f7\x0640"
     L"\x060c\x002f\x2019\x0622\x00d7\x061b\x064e\x064c"
     L"\x064d\x2018\x007b\x064b\x0652\x0625\x007e\x003a"
     L"\x002b\x002c\x005f\x002e\x061f\x0651\x003c\x007c"
     L"\x003e\x0022\x0030\x0031\x0032\x0033\x0034\x0035"
     L"\x0036\x0037\x0038\x0039\x0634\x0624\x064a\x062b"
     L"\x0628\x0644\x0627\x0647\x062a\x0646\x0645\x0629"
     L"\x0649\x062e\x062d\x0636\x0642\x0633\x0641\x0639"
     L"\x0631\x0635\x0621\x063a\x0626\x0643\x003d\x0648"
     L"\x002d\x0632\x0638\x0630\x062c\x005c\x062f\x0637"
    },
    {MockKeyboard::LAYOUT_HEBREW,
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x05e9\x05e0\x05d1\x05d2\x05e7\x05db"
     L"\x05e2\x05d9\x05df\x05d7\x05dc\x05da\x05e6\x05de"
     L"\x05dd\x05e4\x002f\x05e8\x05d3\x05d0\x05d5\x05d4"
     L"\x0027\x05e1\x05d8\x05d6\x05e3\x003d\x05ea\x002d"
     L"\x05e5\x002e\x003b\x005d\x005c\x005b\x002c\x0028"
     L"\x0021\x0040\x0023\x0024\x0025\x005e\x0026\x002a"
     L"\x0029\x0041\x0042\x0043\x0044\x0045\x0046\x0047"
     L"\x0048\x0049\x004a\x004b\x004c\x004d\x004e\x004f"
     L"\x0050\x0051\x0052\x0053\x0054\x0055\x0056\x0057"
     L"\x0058\x0059\x005a\x003a\x002b\x003e\x005f\x003c"
     L"\x003f\x007e\x007d\x007c\x007b\x0022\x0030\x0031"
     L"\x0032\x0033\x0034\x0035\x0036\x0037\x0038\x0039"
     L"\x05e9\x05e0\x05d1\x05d2\x05e7\x05db\x05e2\x05d9"
     L"\x05df\x05d7\x05dc\x05da\x05e6\x05de\x05dd\x05e4"
     L"\x002f\x05e8\x05d3\x05d0\x05d5\x05d4\x0027\x05e1"
     L"\x05d8\x05d6\x05e3\x003d\x05ea\x002d\x05e5\x002e"
     L"\x003b\x005d\x005c\x005b\x002c"
    },
#endif
#if defined(OS_WIN)
    // On Linux, the only way to test alternate keyboard layouts is to change
    // the keyboard layout of the whole screen. I'm worried about the side
    // effects this may have on the buildbots.
    {MockKeyboard::LAYOUT_CANADIAN_FRENCH,
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x0061\x0062\x0063\x0064\x0065\x0066"
     L"\x0067\x0068\x0069\x006a\x006b\x006c\x006d\x006e"
     L"\x006f\x0070\x0071\x0072\x0073\x0074\x0075\x0076"
     L"\x0077\x0078\x0079\x007a\x003b\x003d\x002c\x002d"
     L"\x002e\x00e9\x003c\x0029\x0021\x0022\x002f\x0024"
     L"\x0025\x003f\x0026\x002a\x0028\x0041\x0042\x0043"
     L"\x0044\x0045\x0046\x0047\x0048\x0049\x004a\x004b"
     L"\x004c\x004d\x004e\x004f\x0050\x0051\x0052\x0053"
     L"\x0054\x0055\x0056\x0057\x0058\x0059\x005a\x003a"
     L"\x002b\x0027\x005f\x002e\x00c9\x003e\x0030\x0031"
     L"\x0032\x0033\x0034\x0035\x0036\x0037\x0038\x0039"
     L"\x0061\x0062\x0063\x0064\x0065\x0066\x0067\x0068"
     L"\x0069\x006a\x006b\x006c\x006d\x006e\x006f\x0070"
     L"\x0071\x0072\x0073\x0074\x0075\x0076\x0077\x0078"
     L"\x0079\x007a\x003b\x003d\x002c\x002d\x002e\x00e9"
     L"\x003c"
    },
    {MockKeyboard::LAYOUT_FRENCH,
     L"\x00e0\x0026\x00e9\x0022\x0027\x0028\x002d\x00e8"
     L"\x005f\x00e7\x0061\x0062\x0063\x0064\x0065\x0066"
     L"\x0067\x0068\x0069\x006a\x006b\x006c\x006d\x006e"
     L"\x006f\x0070\x0071\x0072\x0073\x0074\x0075\x0076"
     L"\x0077\x0078\x0079\x007a\x0024\x003d\x002c\x003b"
     L"\x003a\x00f9\x0029\x002a\x0021\x0030\x0031\x0032"
     L"\x0033\x0034\x0035\x0036\x0037\x0038\x0039\x0041"
     L"\x0042\x0043\x0044\x0045\x0046\x0047\x0048\x0049"
     L"\x004a\x004b\x004c\x004d\x004e\x004f\x0050\x0051"
     L"\x0052\x0053\x0054\x0055\x0056\x0057\x0058\x0059"
     L"\x005a\x00a3\x002b\x003f\x002e\x002f\x0025\x00b0"
     L"\x00b5\x00e0\x0026\x00e9\x0022\x0027\x0028\x002d"
     L"\x00e8\x005f\x00e7\x0061\x0062\x0063\x0064\x0065"
     L"\x0066\x0067\x0068\x0069\x006a\x006b\x006c\x006d"
     L"\x006e\x006f\x0070\x0071\x0072\x0073\x0074\x0075"
     L"\x0076\x0077\x0078\x0079\x007a\x0024\x003d\x002c"
     L"\x003b\x003a\x00f9\x0029\x002a\x0021"
    },
    {MockKeyboard::LAYOUT_RUSSIAN,
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x0444\x0438\x0441\x0432\x0443\x0430"
     L"\x043f\x0440\x0448\x043e\x043b\x0434\x044c\x0442"
     L"\x0449\x0437\x0439\x043a\x044b\x0435\x0433\x043c"
     L"\x0446\x0447\x043d\x044f\x0436\x003d\x0431\x002d"
     L"\x044e\x002e\x0451\x0445\x005c\x044a\x044d\x0029"
     L"\x0021\x0022\x2116\x003b\x0025\x003a\x003f\x002a"
     L"\x0028\x0424\x0418\x0421\x0412\x0423\x0410\x041f"
     L"\x0420\x0428\x041e\x041b\x0414\x042c\x0422\x0429"
     L"\x0417\x0419\x041a\x042b\x0415\x0413\x041c\x0426"
     L"\x0427\x041d\x042f\x0416\x002b\x0411\x005f\x042e"
     L"\x002c\x0401\x0425\x002f\x042a\x042d\x0030\x0031"
     L"\x0032\x0033\x0034\x0035\x0036\x0037\x0038\x0039"
     L"\x0444\x0438\x0441\x0432\x0443\x0430\x043f\x0440"
     L"\x0448\x043e\x043b\x0434\x044c\x0442\x0449\x0437"
     L"\x0439\x043a\x044b\x0435\x0433\x043c\x0446\x0447"
     L"\x043d\x044f\x0436\x003d\x0431\x002d\x044e\x002e"
     L"\x0451\x0445\x005c\x044a\x044d"
    },
#endif  // defined(OS_WIN)
    {MockKeyboard::LAYOUT_UNITED_STATES,
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x0061\x0062\x0063\x0064\x0065\x0066"
     L"\x0067\x0068\x0069\x006a\x006b\x006c\x006d\x006e"
     L"\x006f\x0070\x0071\x0072\x0073\x0074\x0075\x0076"
     L"\x0077\x0078\x0079\x007a\x003b\x003d\x002c\x002d"
     L"\x002e\x002f\x0060\x005b\x005c\x005d\x0027\x0029"
     L"\x0021\x0040\x0023\x0024\x0025\x005e\x0026\x002a"
     L"\x0028\x0041\x0042\x0043\x0044\x0045\x0046\x0047"
     L"\x0048\x0049\x004a\x004b\x004c\x004d\x004e\x004f"
     L"\x0050\x0051\x0052\x0053\x0054\x0055\x0056\x0057"
     L"\x0058\x0059\x005a\x003a\x002b\x003c\x005f\x003e"
     L"\x003f\x007e\x007b\x007c\x007d\x0022"
#if defined(OS_WIN)
     // This is ifdefed out for Linux to correspond to the fact that we don't
     // test alt+keystroke for now.
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x0061\x0062\x0063\x0064\x0065\x0066"
     L"\x0067\x0068\x0069\x006a\x006b\x006c\x006d\x006e"
     L"\x006f\x0070\x0071\x0072\x0073\x0074\x0075\x0076"
     L"\x0077\x0078\x0079\x007a\x003b\x003d\x002c\x002d"
     L"\x002e\x002f\x0060\x005b\x005c\x005d\x0027"
#endif
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kLayouts); ++i) {
    // Load an HTML page consisting of one <div> element.
    // This <div> element is used by the EditorClientImpl class to insert
    // characters received through the RenderWidget::OnHandleInputEvent()
    // function.
    view_->set_send_content_state_immediately(true);
    LoadHTML("<html>"
             "<head>"
             "<title></title>"
             "</head>"
             "<body>"
             "<div id='test' contenteditable='true'>"
             "</div>"
             "</body>"
             "</html>");
    ExecuteJavaScript("document.getElementById('test').focus();");
    render_thread_.sink().ClearMessages();

    // For each key code, we send three keyboard events.
    //  * Pressing only the key;
    //  * Pressing the key and a left-shift key, and;
    //  * Pressing the key and a right-alt (AltGr) key.
    static const MockKeyboard::Modifiers kModifiers[] = {
      MockKeyboard::NONE,
      MockKeyboard::LEFT_SHIFT,
#if defined(OS_WIN)
      MockKeyboard::RIGHT_ALT,
#endif
    };

    MockKeyboard::Layout layout = kLayouts[i].layout;
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(kModifiers); ++j) {
      // Virtual key codes used for this test.
      static const int kKeyCodes[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
        'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
        'W', 'X', 'Y', 'Z',
        ui::VKEY_OEM_1,
        ui::VKEY_OEM_PLUS,
        ui::VKEY_OEM_COMMA,
        ui::VKEY_OEM_MINUS,
        ui::VKEY_OEM_PERIOD,
        ui::VKEY_OEM_2,
        ui::VKEY_OEM_3,
        ui::VKEY_OEM_4,
        ui::VKEY_OEM_5,
        ui::VKEY_OEM_6,
        ui::VKEY_OEM_7,
#if defined(OS_WIN)
        // Unclear how to handle this on Linux.
        ui::VKEY_OEM_8,
#endif
      };

      MockKeyboard::Modifiers modifiers = kModifiers[j];
      for (size_t k = 0; k < ARRAYSIZE_UNSAFE(kKeyCodes); ++k) {
        // Send a keyboard event to the RenderView object.
        // We should test a keyboard event only when the given keyboard-layout
        // driver is installed in a PC and the driver can assign a Unicode
        // charcter for the given tuple (layout, key-code, and modifiers).
        int key_code = kKeyCodes[k];
        std::wstring char_code;
        if (SendKeyEvent(layout, key_code, modifiers, &char_code) < 0)
          continue;
      }
    }

    // Retrieve the text in the test page and compare it with the expected
    // text created from a virtual-key code, a character code, and the
    // modifier-key status.
    const int kMaxOutputCharacters = 4096;
    std::wstring output = UTF16ToWideHack(
        GetMainFrame()->contentAsText(kMaxOutputCharacters));
    EXPECT_EQ(kLayouts[i].expected_result, output);
  }
#else
  NOTIMPLEMENTED();
#endif
}

// Crashy, http://crbug.com/53247.
TEST_F(RenderViewTest, DISABLED_DidFailProvisionalLoadWithErrorForError) {
  GetMainFrame()->enableViewSourceMode(true);
  WebURLError error;
  error.domain.fromUTF8("test_domain");
  error.reason = net::ERR_FILE_NOT_FOUND;
  error.unreachableURL = GURL("http://foo");
  WebFrame* web_frame = GetMainFrame();
  // An error occurred.
  view_->didFailProvisionalLoad(web_frame, error);
  // Frame should exit view-source mode.
  EXPECT_FALSE(web_frame->isViewSourceModeEnabled());
}

TEST_F(RenderViewTest, DidFailProvisionalLoadWithErrorForCancellation) {
  GetMainFrame()->enableViewSourceMode(true);
  WebURLError error;
  error.domain.fromUTF8("test_domain");
  error.reason = net::ERR_ABORTED;
  error.unreachableURL = GURL("http://foo");
  WebFrame* web_frame = GetMainFrame();
  // A cancellation occurred.
  view_->didFailProvisionalLoad(web_frame, error);
  // Frame should stay in view-source mode.
  EXPECT_TRUE(web_frame->isViewSourceModeEnabled());
}

// Regression test for http://crbug.com/35011
TEST_F(RenderViewTest, JSBlockSentAfterPageLoad) {
  // 1. Load page with JS.
  std::string html = "<html>"
           "<head>"
           "<script>document.createElement('div');</script>"
           "</head>"
           "<body>"
           "</body>"
           "</html>";
  render_thread_.sink().ClearMessages();
  LoadHTML(html.c_str());

  // 2. Block JavaScript.
  ContentSettings settings;
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
    settings.settings[i] = CONTENT_SETTING_ALLOW;
  settings.settings[CONTENT_SETTINGS_TYPE_JAVASCRIPT] = CONTENT_SETTING_BLOCK;
  view_->SetContentSettings(settings);

  // Make sure no pending messages are in the queue.
  ProcessPendingMessages();
  render_thread_.sink().ClearMessages();

  // 3. Reload page.
  ViewMsg_Navigate_Params params;
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(html);
  GURL url(url_str);
  params.url = url;
  params.navigation_type = ViewMsg_Navigate_Params::RELOAD;
  view_->OnNavigate(params);
  ProcessPendingMessages();

  // 4. Verify that the notification that javascript was blocked is sent after
  //    the navigation notifiction is sent.
  int navigation_index = -1;
  int block_index = -1;
  for (size_t i = 0; i < render_thread_.sink().message_count(); ++i) {
    const IPC::Message* msg = render_thread_.sink().GetMessageAt(i);
    if (msg->type() == ViewHostMsg_FrameNavigate::ID)
      navigation_index = i;
    if (msg->type() == ViewHostMsg_ContentBlocked::ID)
      block_index = i;
  }
  EXPECT_NE(-1, navigation_index);
  EXPECT_NE(-1, block_index);
  EXPECT_LT(navigation_index, block_index);
}

// Regression test for http://crbug.com/41562
TEST_F(RenderViewTest, UpdateTargetURLWithInvalidURL) {
  const GURL invalid_gurl("http://");
  view_->setMouseOverURL(WebKit::WebURL(invalid_gurl));
  EXPECT_EQ(invalid_gurl, view_->target_url_);
}

TEST_F(RenderViewTest, SendForms) {
  // Don't want any delay for form state sync changes. This will still post a
  // message so updates will get coalesced, but as soon as we spin the message
  // loop, it will generate an update.
  view_->set_send_content_state_immediately(true);

  LoadHTML("<form method=\"POST\">"
           "  <input type=\"text\" id=\"firstname\"/>"
           "  <input type=\"text\" id=\"middlename\" autoComplete=\"off\"/>"
           "  <input type=\"hidden\" id=\"lastname\"/>"
           "  <select id=\"state\"/>"
           "    <option>?</option>"
           "    <option>California</option>"
           "    <option>Texas</option>"
           "  </select>"
           "</form>");

  // Verify that "FormsSeen" sends the expected number of fields.
  ProcessPendingMessages();
  const IPC::Message* message = render_thread_.sink().GetFirstMessageMatching(
      AutoFillHostMsg_FormsSeen::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  AutoFillHostMsg_FormsSeen::Param params;
  AutoFillHostMsg_FormsSeen::Read(message, &params);
  const std::vector<FormData>& forms = params.a;
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(3UL, forms[0].fields.size());
  EXPECT_TRUE(forms[0].fields[0].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("firstname"),
                string16(),
                ASCIIToUTF16("text"),
                kDefaultMaxLength,
                false))) << forms[0].fields[0];
  EXPECT_TRUE(forms[0].fields[1].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("middlename"),
                string16(),
                ASCIIToUTF16("text"),
                kDefaultMaxLength,
                false))) << forms[0].fields[1];
  EXPECT_TRUE(forms[0].fields[2].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("state"),
                ASCIIToUTF16("?"),
                ASCIIToUTF16("select-one"),
                0,
                false))) << forms[0].fields[2];

  // Verify that |didAcceptAutoFillSuggestion()| sends the expected number of
  // fields.
  WebFrame* web_frame = GetMainFrame();
  WebDocument document = web_frame->document();
  WebInputElement firstname =
      document.getElementById("firstname").to<WebInputElement>();

  // Accept suggestion that contains a label.  Labeled items indicate AutoFill
  // as opposed to Autocomplete.  We're testing this distinction below with
  // the |AutoFillHostMsg_FillAutoFillFormData::ID| message.
  autofill_agent_->didAcceptAutoFillSuggestion(
      firstname,
      WebKit::WebString::fromUTF8("Johnny"),
      WebKit::WebString::fromUTF8("Home"),
      1,
      -1);

  ProcessPendingMessages();
  const IPC::Message* message2 = render_thread_.sink().GetUniqueMessageMatching(
      AutoFillHostMsg_FillAutoFillFormData::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message2);
  AutoFillHostMsg_FillAutoFillFormData::Param params2;
  AutoFillHostMsg_FillAutoFillFormData::Read(message2, &params2);
  const FormData& form2 = params2.b;
  ASSERT_EQ(3UL, form2.fields.size());
  EXPECT_TRUE(form2.fields[0].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("firstname"),
                string16(),
                ASCIIToUTF16("text"),
                kDefaultMaxLength,
                false))) << form2.fields[0];
  EXPECT_TRUE(form2.fields[1].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("middlename"),
                string16(),
                ASCIIToUTF16("text"),
                kDefaultMaxLength,
                false))) << form2.fields[1];
  EXPECT_TRUE(form2.fields[2].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("state"),
                ASCIIToUTF16("?"),
                ASCIIToUTF16("select-one"),
                0,
                false))) << form2.fields[2];
}

TEST_F(RenderViewTest, FillFormElement) {
  // Don't want any delay for form state sync changes. This will still post a
  // message so updates will get coalesced, but as soon as we spin the message
  // loop, it will generate an update.
  view_->set_send_content_state_immediately(true);

  LoadHTML("<form method=\"POST\">"
           "  <input type=\"text\" id=\"firstname\"/>"
           "  <input type=\"text\" id=\"middlename\"/>"
           "</form>");

  // Verify that "FormsSeen" isn't sent, as there are too few fields.
  ProcessPendingMessages();
  const IPC::Message* message = render_thread_.sink().GetFirstMessageMatching(
      AutoFillHostMsg_FormsSeen::ID);
  ASSERT_EQ(static_cast<IPC::Message*>(NULL), message);

  // Verify that |didAcceptAutoFillSuggestion()| sets the value of the expected
  // field.
  WebFrame* web_frame = GetMainFrame();
  WebDocument document = web_frame->document();
  WebInputElement firstname =
      document.getElementById("firstname").to<WebInputElement>();
  WebInputElement middlename =
      document.getElementById("middlename").to<WebInputElement>();
  middlename.setAutofilled(true);

  // Accept a suggestion in a form that has been auto-filled.  This triggers
  // the direct filling of the firstname element with value parameter.
  autofill_agent_->didAcceptAutoFillSuggestion(firstname,
                                               WebString::fromUTF8("David"),
                                               WebString(),
                                               0,
                                               0);

  ProcessPendingMessages();
  const IPC::Message* message2 = render_thread_.sink().GetUniqueMessageMatching(
      AutoFillHostMsg_FillAutoFillFormData::ID);

  // No message should be sent in this case.  |firstname| is filled directly.
  ASSERT_EQ(static_cast<IPC::Message*>(NULL), message2);
  EXPECT_EQ(firstname.value(), WebKit::WebString::fromUTF8("David"));
}

// Tests that we send the right translatable for a page and that we respect the
// "no translate" meta-tag.
TEST_F(RenderViewTest, TranslatablePage) {
  // Suppress the normal delay that occurs when the page is loaded before which
  // the renderer sends the page contents to the browser.
  view_->set_send_content_state_immediately(true);

  LoadHTML("<html><body>A random page with random content.</body></html>");
  ProcessPendingMessages();
  const IPC::Message* message = render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_PageContents::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ViewHostMsg_PageContents::Param params;
  ViewHostMsg_PageContents::Read(message, &params);
  EXPECT_TRUE(params.e);  // Translatable should be true.
  render_thread_.sink().ClearMessages();

  // Now the page specifies the META tag to prevent translation.
  LoadHTML("<html><head><meta name=\"google\" value=\"notranslate\"></head>"
           "<body>A random page with random content.</body></html>");
  ProcessPendingMessages();
  message = render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_PageContents::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ViewHostMsg_PageContents::Read(message, &params);
  EXPECT_FALSE(params.e);  // Translatable should be false.
  render_thread_.sink().ClearMessages();

  // Try the alternate version of the META tag (content instead of value).
  LoadHTML("<html><head><meta name=\"google\" content=\"notranslate\"></head>"
           "<body>A random page with random content.</body></html>");
  ProcessPendingMessages();
  message = render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_PageContents::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ViewHostMsg_PageContents::Read(message, &params);
  EXPECT_FALSE(params.e);  // Translatable should be false.
}

// Tests that the language meta tag takes precedence over the CLD when reporting
// the page's language.
TEST_F(RenderViewTest, LanguageMetaTag) {
  // Suppress the normal delay that occurs when the page is loaded before which
  // the renderer sends the page contents to the browser.
  view_->set_send_content_state_immediately(true);

  LoadHTML("<html><head><meta http-equiv=\"content-language\" content=\"es\">"
           "</head><body>A random page with random content.</body></html>");
  ProcessPendingMessages();
  const IPC::Message* message = render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_PageContents::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ViewHostMsg_PageContents::Param params;
  ViewHostMsg_PageContents::Read(message, &params);
  EXPECT_EQ("es", params.d);
  render_thread_.sink().ClearMessages();

  // Makes sure we support multiple languages specified.
  LoadHTML("<html><head><meta http-equiv=\"content-language\" "
           "content=\" fr , es,en \">"
           "</head><body>A random page with random content.</body></html>");
  ProcessPendingMessages();
  message = render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_PageContents::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ViewHostMsg_PageContents::Read(message, &params);
  EXPECT_EQ("fr", params.d);
}
