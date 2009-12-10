// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/keyboard_codes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

const wchar_t kTestingPage[] = L"files/keyevents_test.html";
const wchar_t kSuppressEventJS[] =
    L"window.domAutomationController.send(setDefaultAction('%ls', %ls));";
const wchar_t kGetResultJS[] =
    L"window.domAutomationController.send(keyEventResult[%d]);";
const wchar_t kGetResultLengthJS[] =
    L"window.domAutomationController.send(keyEventResult.length);";
const wchar_t kGetFocusedElementJS[] =
    L"window.domAutomationController.send(focusedElement);";
const wchar_t kSetFocusedElementJS[] =
    L"window.domAutomationController.send(setFocusedElement('%ls'));";
const wchar_t kGetTextBoxValueJS[] =
    L"window.domAutomationController.send("
    L"document.getElementById('%ls').value);";
const wchar_t kStartTestJS[] =
    L"window.domAutomationController.send(startTest());";

// Maximum lenght of the result array in KeyEventTestData structure.
const size_t kMaxResultLength = 10;

// A structure holding test data of a keyboard event.
// Each keyboard event may generate multiple result strings representing
// the result of keydown, keypress, keyup and textInput events.
// For keydown, keypress and keyup events, the format of the result string is:
// <type> <keyCode> <charCode> <ctrlKey> <shiftKey> <altKey>
// where <type> may be 'D' (keydown), 'P' (keypress) or 'U' (keyup).
// <ctrlKey>, <shiftKey> and <altKey> are boolean value indicating the state of
// corresponding modifier key.
// For textInput event, the format is: T <text>, where <text> is the text to be
// input.
// Please refer to chrome/test/data/keyevents_test.html for details.
struct KeyEventTestData {
  base::KeyboardCode key;
  bool ctrl;
  bool shift;
  bool alt;

  bool suppress_keydown;
  bool suppress_keypress;
  bool suppress_keyup;
  bool suppress_textinput;

  int result_length;
  const char* const result[kMaxResultLength];
};

const wchar_t* GetBoolString(bool value) {
  return value ? L"true" : L"false";
}

// A class to help wait for the finish of a key event test.
class TestFinishObserver : public NotificationObserver {
 public:
  explicit TestFinishObserver(RenderViewHost* render_view_host)
      : finished_(false), waiting_(false) {
    registrar_.Add(this, NotificationType::DOM_OPERATION_RESPONSE,
                   Source<RenderViewHost>(render_view_host));
  }

  bool WaitForFinish() {
    if (!finished_) {
      waiting_ = true;
      ui_test_utils::RunMessageLoop();
      waiting_ = false;
    }
    return finished_;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::DOM_OPERATION_RESPONSE);
    Details<DomOperationNotificationDetails> dom_op_details(details);
    // We might receive responses for other script execution, but we only
    // care about the test finished message.
    if (dom_op_details->json() == "\"FINISHED\"") {
      finished_ = true;
      if (waiting_)
        MessageLoopForUI::current()->Quit();
    }
  }

 private:
  bool finished_;
  bool waiting_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TestFinishObserver);
};

class BrowserKeyEventsTest : public InProcessBrowserTest {
 public:
  BrowserKeyEventsTest() {
    set_show_window(true);
    EnableDOMAutomation();
  }

  void GetNativeWindow(gfx::NativeWindow* native_window) {
    BrowserWindow* window = browser()->window();
    ASSERT_TRUE(window);
    *native_window = window->GetNativeHandle();
    ASSERT_TRUE(*native_window);
  }

  void SendKey(base::KeyboardCode key, bool control, bool shift, bool alt) {
    gfx::NativeWindow window = NULL;
    ASSERT_NO_FATAL_FAILURE(GetNativeWindow(&window));
    ui_controls::SendKeyPressNotifyWhenDone(window, key, control, shift, alt,
                                            new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
  }

  bool IsViewFocused(ViewID vid) {
    return ui_test_utils::IsViewFocused(browser(), vid);
  }

  void ClickOnView(ViewID vid) {
    ui_test_utils::ClickOnView(browser(), vid);
  }

  // Set the suppress flag of an event specified by |type|. If |suppress| is
  // true then the web page will suppress all events with |type|. Following
  // event types are supported: keydown, keypress, keyup and textInput.
  void SuppressEventByType(int tab_index, const wchar_t* type, bool suppress) {
    ASSERT_LT(tab_index, browser()->tab_count());
    bool actual;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetTabContentsAt(tab_index)->render_view_host(),
        L"",
        StringPrintf(kSuppressEventJS, type, GetBoolString(!suppress)),
        &actual));
    ASSERT_EQ(!suppress, actual);
  }

  void SuppressEvents(int tab_index, bool keydown, bool keypress,
                      bool keyup, bool textinput) {
    ASSERT_NO_FATAL_FAILURE(
        SuppressEventByType(tab_index, L"keydown", keydown));
    ASSERT_NO_FATAL_FAILURE(
        SuppressEventByType(tab_index, L"keypress", keypress));
    ASSERT_NO_FATAL_FAILURE(
        SuppressEventByType(tab_index, L"keyup", keyup));
    ASSERT_NO_FATAL_FAILURE(
        SuppressEventByType(tab_index, L"textInput", textinput));
  }

  void SuppressAllEvents(int tab_index, bool suppress) {
    SuppressEvents(tab_index, suppress, suppress, suppress, suppress);
  }

  void GetResultLength(int tab_index, int* length) {
    ASSERT_LT(tab_index, browser()->tab_count());
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
        browser()->GetTabContentsAt(tab_index)->render_view_host(),
        L"", kGetResultLengthJS, length));
  }

  void CheckResult(int tab_index, int length, const char* const result[]) {
    ASSERT_LT(tab_index, browser()->tab_count());
    int actual_length;
    ASSERT_NO_FATAL_FAILURE(GetResultLength(tab_index, &actual_length));
    ASSERT_GE(actual_length, length);
    for (int i = 0; i < actual_length; ++i) {
      std::string actual;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetTabContentsAt(tab_index)->render_view_host(),
          L"", StringPrintf(kGetResultJS, i), &actual));

      // If more events were received than expected, then the additional events
      // must be keyup events.
      if (i < length)
        ASSERT_STREQ(result[i], actual.c_str());
      else
        ASSERT_EQ('U', actual[0]);
    }
  }

  void CheckFocusedElement(int tab_index, const wchar_t* focused) {
    ASSERT_LT(tab_index, browser()->tab_count());
    std::string actual;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
        browser()->GetTabContentsAt(tab_index)->render_view_host(),
        L"", kGetFocusedElementJS, &actual));
    ASSERT_EQ(WideToUTF8(focused), actual);
  }

  void SetFocusedElement(int tab_index, const wchar_t* focused) {
    ASSERT_LT(tab_index, browser()->tab_count());
    bool actual;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetTabContentsAt(tab_index)->render_view_host(),
        L"",
        StringPrintf(kSetFocusedElementJS, focused),
        &actual));
    ASSERT_TRUE(actual);
  }

  void CheckTextBoxValue(int tab_index, const wchar_t* id,
                         const wchar_t* value) {
    ASSERT_LT(tab_index, browser()->tab_count());
    std::string actual;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
        browser()->GetTabContentsAt(tab_index)->render_view_host(),
        L"",
        StringPrintf(kGetTextBoxValueJS, id),
        &actual));
    ASSERT_EQ(WideToUTF8(value), actual);
  }

  void StartTest(int tab_index) {
    ASSERT_LT(tab_index, browser()->tab_count());
    bool actual;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetTabContentsAt(tab_index)->render_view_host(),
        L"", kStartTestJS, &actual));
    ASSERT_TRUE(actual);
  }

  void TestKeyEvent(int tab_index, const KeyEventTestData& test) {
    ASSERT_LT(tab_index, browser()->tab_count());
    ASSERT_EQ(tab_index, browser()->selected_index());

    // Inform our testing web page that we are about to start testing a key
    // event.
    ASSERT_NO_FATAL_FAILURE(StartTest(tab_index));
    ASSERT_NO_FATAL_FAILURE(SuppressEvents(
        tab_index, test.suppress_keydown, test.suppress_keypress,
        test.suppress_keyup, test.suppress_textinput));

    // We need to create a finish observer before sending the key event,
    // because the test finished message might be arrived before returning
    // from the SendKey() method.
    TestFinishObserver finish_observer(
        browser()->GetTabContentsAt(tab_index)->render_view_host());

    ASSERT_NO_FATAL_FAILURE(SendKey(test.key, test.ctrl, test.shift, test.alt));
    ASSERT_TRUE(finish_observer.WaitForFinish());
    ASSERT_NO_FATAL_FAILURE(CheckResult(
        tab_index, test.result_length, test.result));
  }

  std::string GetTestDataDescription(const KeyEventTestData& data) {
    std::string desc = StringPrintf(
        " VKEY:0x%02x, ctrl:%d, shift:%d, alt:%d\n"
        " Suppress: keydown:%d, keypress:%d, keyup:%d, textInput:%d\n"
        " Expected results(%d):\n",
        data.key, data.ctrl, data.shift, data.alt,
        data.suppress_keydown, data.suppress_keypress, data.suppress_keyup,
        data.suppress_textinput, data.result_length);
    for (int i = 0; i < data.result_length; ++i) {
      desc.append("  ");
      desc.append(data.result[i]);
      desc.append("\n");
    }
    return desc;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, NormalKeyEvents) {
  static const KeyEventTestData kTestNoInput[] = {
    // a
    { base::VKEY_A, false, false, false,
      false, false, false, false, 3,
      { "D 65 0 false false false",
        "P 97 97 false false false",
        "U 65 0 false false false" } },
    // shift-a
    { base::VKEY_A, false, true, false,
      false, false, false, false, 5,
      { "D 16 0 false true false",
        "D 65 0 false true false",
        "P 65 65 false true false",
        "U 65 0 false true false",
        "U 16 0 false true false" } },
    // a, suppress keydown
    { base::VKEY_A, false, false, false,
      true, false, false, false, 2,
      { "D 65 0 false false false",
        "U 65 0 false false false" } },
  };

  static const KeyEventTestData kTestWithInput[] = {
    // a
    { base::VKEY_A, false, false, false,
      false, false, false, false, 4,
      { "D 65 0 false false false",
        "P 97 97 false false false",
        "T a",
        "U 65 0 false false false" } },
    // shift-a
    { base::VKEY_A, false, true, false,
      false, false, false, false, 6,
      { "D 16 0 false true false",
        "D 65 0 false true false",
        "P 65 65 false true false",
        "T A",
        "U 65 0 false true false",
        "U 16 0 false true false" } },
    // a, suppress keydown
    { base::VKEY_A, false, false, false,
      true, false, false, false, 2,
      { "D 65 0 false false false",
        "U 65 0 false false false" } },
    // a, suppress keypress
    { base::VKEY_A, false, false, false,
      false, true, false, false, 3,
      { "D 65 0 false false false",
        "P 97 97 false false false",
        "U 65 0 false false false" } },
    // a, suppress textInput
    { base::VKEY_A, false, false, false,
      false, false, false, true, 4,
      { "D 65 0 false false false",
        "P 97 97 false false false",
        "T a",
        "U 65 0 false false false" } },
  };

  HTTPTestServer* server = StartHTTPServer();

  GURL url = server->TestServerPageW(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  int tab_index = browser()->selected_index();
  for (size_t i = 0; i < arraysize(kTestNoInput); ++i) {
    EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestNoInput[i]))
        << "kTestNoInput[" << i << "] failed:\n"
        << GetTestDataDescription(kTestNoInput[i]);
  }

  ASSERT_NO_FATAL_FAILURE(SetFocusedElement(tab_index, L"A"));
  for (size_t i = 0; i < arraysize(kTestWithInput); ++i) {
    EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestWithInput[i]))
        << "kTestWithInput[" << i << "] failed:\n"
        << GetTestDataDescription(kTestWithInput[i]);
  }

  EXPECT_NO_FATAL_FAILURE(CheckTextBoxValue(tab_index, L"A", L"aA"));
}

IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, CtrlKeyEvents) {
  static const KeyEventTestData kTestCtrlF = {
    base::VKEY_F, true, false, false,
    false, false, false, false, 2,
    { "D 17 0 true false false",
      "D 70 0 true false false" }
  };

  static const KeyEventTestData kTestCtrlFSuppressKeyDown = {
    base::VKEY_F, true, false, false,
    true, false, false, false, 4,
    { "D 17 0 true false false",
      "D 70 0 true false false",
      "U 70 0 true false false",
      "U 17 0 true false false" }
  };

  // Ctrl+Z doesn't bind to any accelerators, which then should generate a
  // keypress event with charCode=26.
  static const KeyEventTestData kTestCtrlZ = {
    base::VKEY_Z, true, false, false,
    false, false, false, false, 5,
    { "D 17 0 true false false",
      "D 90 0 true false false",
      "P 26 26 true false false",
      "U 90 0 true false false",
      "U 17 0 true false false" }
  };

  static const KeyEventTestData kTestCtrlZSuppressKeyDown = {
    base::VKEY_Z, true, false, false,
    true, false, false, false, 4,
    { "D 17 0 true false false",
      "D 90 0 true false false",
      "U 90 0 true false false",
      "U 17 0 true false false" }
  };

  // Ctrl+Enter shall generate a keypress event with charCode=10 (LF).
  static const KeyEventTestData kTestCtrlEnter = {
    base::VKEY_RETURN, true, false, false,
    false, false, false, false, 5,
    { "D 17 0 true false false",
      "D 13 0 true false false",
      "P 10 10 true false false",
      "U 13 0 true false false",
      "U 17 0 true false false" }
  };

  HTTPTestServer* server = StartHTTPServer();

  GURL url = server->TestServerPageW(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  int tab_index = browser()->selected_index();
  // Press Ctrl+F, which will make the Find box open and request focus.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlF));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Press Escape to close the Find box and move the focus back to the web page.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_ESCAPE, false, false, false));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Press Ctrl+F with keydown suppressed shall not open the find box.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlFSuppressKeyDown));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlZ));
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlZSuppressKeyDown));
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlEnter));
}

IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, AccessKeys) {
  static const KeyEventTestData kTestAltA = {
    base::VKEY_A, false, false, true,
    false, false, false, false, 4,
    { "D 18 0 false false true",
      "D 65 0 false false true",
      "U 65 0 false false true",
      "U 18 0 false false true" }
  };

  static const KeyEventTestData kTestAltD = {
    base::VKEY_D, false, false, true,
    false, false, false, false, 2,
    { "D 18 0 false false true",
      "D 68 0 false false true" }
  };

  static const KeyEventTestData kTestAltDSuppress = {
    base::VKEY_D, false, false, true,
    true, true, true, false, 4,
    { "D 18 0 false false true",
      "D 68 0 false false true",
      "U 68 0 false false true",
      "U 18 0 false false true" }
  };

  static const KeyEventTestData kTestAlt1 = {
    base::VKEY_1, false, false, true,
    false, false, false, false, 4,
    { "D 18 0 false false true",
      "D 49 0 false false true",
      "U 49 0 false false true",
      "U 18 0 false false true" }
  };

  HTTPTestServer* server = StartHTTPServer();

  GURL url = server->TestServerPageW(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  int tab_index = browser()->selected_index();
  // Make sure no element is focused.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));
  // Alt+A should focus the element with accesskey = "A".
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestAltA));
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L"A"));

  // Blur the focused element.
  EXPECT_NO_FATAL_FAILURE(SetFocusedElement(tab_index, L""));
  // Make sure no element is focused.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));
  // Alt+D should move the focus to the location entry.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestAltD));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
  // No element should be focused, as Alt+D was handled by the browser.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));

  // Move the focus back to the web page.
  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Make sure no element is focused.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));
  // If the keydown event is suppressed, then Alt+D should be handled as an
  // accesskey rather than an accelerator key. Activation of an accesskey is not
  // a part of the default action of the key event, so it should not be
  // suppressed at all.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestAltDSuppress));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L"D"));

  // Blur the focused element.
  EXPECT_NO_FATAL_FAILURE(SetFocusedElement(tab_index, L""));
  // Make sure no element is focused.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestAlt1));
#if defined(OS_LINUX)
  // On Linux, alt-0..9 are assigned as tab selection accelerators, so they can
  // not be used as accesskeys.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));
#elif defined(OS_WIN)
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L"1"));
#endif
}

IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, ReservedAccelerators) {
  HTTPTestServer* server = StartHTTPServer();

  GURL url = server->TestServerPageW(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

#if defined(OS_WIN)
  static const KeyEventTestData kTestCtrlT = {
    base::VKEY_T, true, false, false,
    true, false, false, false, 1,
    { "D 17 0 true false false" }
  };

  ASSERT_EQ(1, browser()->tab_count());
  // Press Ctrl+T, which will open a new tab.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(0, kTestCtrlT));
  EXPECT_EQ(2, browser()->tab_count());
  browser()->SelectNumberedTab(0);
  ASSERT_EQ(0, browser()->selected_index());

  int result_length;
  ASSERT_NO_FATAL_FAILURE(GetResultLength(0, &result_length));
  EXPECT_EQ(1, result_length);

  // Reserved accelerators can't be suppressed.
  ASSERT_NO_FATAL_FAILURE(SuppressAllEvents(0, true));
  // Press Ctrl+W, which will close the tab.
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_W, true, false, false));
  EXPECT_EQ(1, browser()->tab_count());

#elif defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
  // Ctrl-[a-z] are not treated as reserved accelerators on Linux.
  static const KeyEventTestData kTestCtrlT = {
    base::VKEY_T, true, false, false,
    false, false, false, false, 2,
    { "D 17 0 true false false",
      "D 84 0 true false false" }
  };

  static const KeyEventTestData kTestCtrlPageDown = {
    base::VKEY_NEXT, true, false, false,
    true, false, false, false, 1,
    { "D 17 0 true false false" }
  };

  static const KeyEventTestData kTestCtrlTab = {
    base::VKEY_TAB, true, false, false,
    true, false, false, false, 1,
    { "D 17 0 true false false" }
  };

  static const KeyEventTestData kTestCtrlTBlocked = {
    base::VKEY_T, true, false, false,
    true, false, false, false, 4,
    { "D 17 0 true false false",
      "D 84 0 true false false",
      "U 84 0 true false false",
      "U 17 0 true false false" }
  };

  static const KeyEventTestData kTestCtrlWBlocked = {
    base::VKEY_W, true, false, false,
    true, false, false, false, 4,
    { "D 17 0 true false false",
      "D 87 0 true false false",
      "U 87 0 true false false",
      "U 17 0 true false false" }
  };

  ASSERT_EQ(1, browser()->tab_count());

  // Ctrl+T should be blockable.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(0, kTestCtrlTBlocked));
  ASSERT_EQ(1, browser()->tab_count());

  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(0, kTestCtrlT));
  ASSERT_EQ(2, browser()->tab_count());
  ASSERT_EQ(1, browser()->selected_index());
  browser()->SelectNumberedTab(0);
  ASSERT_EQ(0, browser()->selected_index());

  // Ctrl+PageDown and Ctrl+Tab switches to the next tab.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(0, kTestCtrlPageDown));
  ASSERT_EQ(1, browser()->selected_index());

  browser()->SelectNumberedTab(0);
  ASSERT_EQ(0, browser()->selected_index());
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(0, kTestCtrlTab));
  ASSERT_EQ(1, browser()->selected_index());

  // Ctrl+W should be blockable.
  browser()->SelectNumberedTab(0);
  ASSERT_EQ(0, browser()->selected_index());
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(0, kTestCtrlWBlocked));
  ASSERT_EQ(2, browser()->tab_count());

  // Ctrl+F4 to close the tab.
  ASSERT_NO_FATAL_FAILURE(SuppressAllEvents(0, true));
  ASSERT_NO_FATAL_FAILURE(SendKey(base::VKEY_F4, true, false, false));
  ASSERT_EQ(1, browser()->tab_count());
#endif
}
