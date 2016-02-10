// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/validation.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::ASCIIToUTF16;

namespace autofill {

static const char kDataURIPrefix[] = "data:text/html;charset=utf-8,";
static const char kTestFormString[] =
    "<form action=\"http://www.example.com/\" method=\"POST\">"
    "<label for=\"firstname\">First name:</label>"
    " <input type=\"text\" id=\"firstname\""
    "        onfocus=\"domAutomationController.send(true)\"><br>"
    "<label for=\"lastname\">Last name:</label>"
    " <input type=\"text\" id=\"lastname\"><br>"
    "<label for=\"address1\">Address line 1:</label>"
    " <input type=\"text\" id=\"address1\"><br>"
    "<label for=\"address2\">Address line 2:</label>"
    " <input type=\"text\" id=\"address2\"><br>"
    "<label for=\"city\">City:</label>"
    " <input type=\"text\" id=\"city\"><br>"
    "<label for=\"state\">State:</label>"
    " <select id=\"state\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">California</option>"
    " <option value=\"TX\">Texas</option>"
    " </select><br>"
    "<label for=\"zip\">ZIP code:</label>"
    " <input type=\"text\" id=\"zip\"><br>"
    "<label for=\"country\">Country:</label>"
    " <select id=\"country\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">Canada</option>"
    " <option value=\"US\">United States</option>"
    " </select><br>"
    "<label for=\"phone\">Phone number:</label>"
    " <input type=\"text\" id=\"phone\"><br>"
    "</form>";
static const char kTestPasswordFormString[] =
    "<form>"
    "<label for=\"user\">User:</label>"
    " <input id=\"user\" type=\"text\" name=\"name\""
             "onfocus=\"domAutomationController.send(true)\">"
    "<br>"
    "<label for=\"password\">Password:</label>"
    " <input id=\"password\" type=\"password\" name=\"password\""
             "onfocus=\"domAutomationController.send(true)\">"
    "<br>"
    "<input type=\"submit\" value=\"Submit\">"
    "</form>";

// AutofillManagerTestDelegateImpl --------------------------------------------

class AutofillManagerTestDelegateImpl
    : public autofill::AutofillManagerTestDelegate {
 public:
  AutofillManagerTestDelegateImpl()
      : waiting_for_text_change_(false) {}
  ~AutofillManagerTestDelegateImpl() override {}

  // autofill::AutofillManagerTestDelegate:
  void DidPreviewFormData() override {
    ASSERT_TRUE(loop_runner_->loop_running());
    loop_runner_->Quit();
  }

  void DidFillFormData() override {
    ASSERT_TRUE(loop_runner_->loop_running());
    loop_runner_->Quit();
  }

  void DidShowSuggestions() override {
    ASSERT_TRUE(loop_runner_->loop_running());
    loop_runner_->Quit();
  }

  void OnTextFieldChanged() override {
    if (!waiting_for_text_change_)
      return;
    waiting_for_text_change_ = false;
    ASSERT_TRUE(loop_runner_->loop_running());
    loop_runner_->Quit();
  }

  void Reset() {
    loop_runner_ = new content::MessageLoopRunner();
  }

  void Wait() {
    loop_runner_->Run();
  }

  void WaitForTextChange() {
    waiting_for_text_change_ = true;
    loop_runner_->Run();
  }

 private:
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  bool waiting_for_text_change_;

  DISALLOW_COPY_AND_ASSIGN(AutofillManagerTestDelegateImpl);
};

// AutofillInteractiveTest ----------------------------------------------------

class AutofillInteractiveTest : public InProcessBrowserTest {
 protected:
  AutofillInteractiveTest() :
      key_press_event_sink_(
          base::Bind(&AutofillInteractiveTest::HandleKeyPressEvent,
                     base::Unretained(this))) {}
  ~AutofillInteractiveTest() override {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    // Don't want Keychain coming up on Mac.
    test::DisableSystemServices(browser()->profile()->GetPrefs());

    // Inject the test delegate into the AutofillManager.
    content::WebContents* web_contents = GetWebContents();
    ContentAutofillDriver* autofill_driver =
        ContentAutofillDriverFactory::FromWebContents(web_contents)
            ->DriverForFrame(web_contents->GetMainFrame());
    AutofillManager* autofill_manager = autofill_driver->autofill_manager();
    autofill_manager->SetTestDelegate(&test_delegate_);

    // If the mouse happened to be over where the suggestions are shown, then
    // the preview will show up and will fail the tests. We need to give it a
    // point that's within the browser frame, or else the method hangs.
    gfx::Point reset_mouse(GetWebContents()->GetContainerBounds().origin());
    reset_mouse = gfx::Point(reset_mouse.x() + 5, reset_mouse.y() + 5);
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(reset_mouse));

    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // Make sure to close any showing popups prior to tearing down the UI.
    content::WebContents* web_contents = GetWebContents();
    AutofillManager* autofill_manager =
        ContentAutofillDriverFactory::FromWebContents(web_contents)
            ->DriverForFrame(web_contents->GetMainFrame())
            ->autofill_manager();
    autofill_manager->client()->HideAutofillPopup();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderViewHost* GetRenderViewHost() {
    return GetWebContents()->GetRenderViewHost();
  }

  void CreateTestProfile() {
    AutofillProfile profile;
    test::SetProfileInfo(
        &profile, "Milton", "C.", "Waddams",
        "red.swingline@initech.com", "Initech", "4120 Freidrich Lane",
        "Basement", "Austin", "Texas", "78744", "US", "5125551234");

    AddTestProfile(browser(), profile);
  }

  // Populates a webpage form using autofill data and keypress events.
  // This function focuses the specified input field in the form, and then
  // sends keypress events to the tab to cause the form to be populated.
  void PopulateForm(const std::string& field_id) {
    std::string js("document.getElementById('" + field_id + "').focus();");
    ASSERT_TRUE(content::ExecuteScript(GetRenderViewHost(), js));

    SendKeyToPageAndWait(ui::VKEY_DOWN);
    SendKeyToPopupAndWait(ui::VKEY_DOWN);
    SendKeyToPopupAndWait(ui::VKEY_RETURN);
  }

  void ExpectFieldValue(const std::string& field_name,
                        const std::string& expected_value) {
    std::string value;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        GetWebContents(),
        "window.domAutomationController.send("
        "    document.getElementById('" + field_name + "').value);",
        &value));
    EXPECT_EQ(expected_value, value) << "for field " << field_name;
  }

  void GetFieldBackgroundColor(const std::string& field_name,
                               std::string* color) {
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        GetWebContents(),
        "window.domAutomationController.send("
        "    document.defaultView.getComputedStyle(document.getElementById('" +
        field_name + "')).backgroundColor);",
        color));
  }

  void SimulateURLFetch(bool success) {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    net::Error error = success ? net::OK : net::ERR_FAILED;

    std::string script = " var google = {};"
        "google.translate = (function() {"
        "  return {"
        "    TranslateService: function() {"
        "      return {"
        "        isAvailable : function() {"
        "          return true;"
        "        },"
        "        restore : function() {"
        "          return;"
        "        },"
        "        getDetectedLanguage : function() {"
        "          return \"ja\";"
        "        },"
        "        translatePage : function(originalLang, targetLang,"
        "                                 onTranslateProgress) {"
        "          document.getElementsByTagName(\"body\")[0].innerHTML = '" +
        std::string(kTestFormString) +
        "              ';"
        "          onTranslateProgress(100, true, false);"
        "        }"
        "      };"
        "    }"
        "  };"
        "})();"
        "cr.googleTranslate.onTranslateElementLoad();";

    fetcher->set_url(fetcher->GetOriginalURL());
    fetcher->set_status(net::URLRequestStatus::FromError(error));
    fetcher->set_response_code(success ? 200 : 500);
    fetcher->SetResponseString(script);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void FocusFieldByName(const std::string& name) {
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        GetRenderViewHost(),
        "if (document.readyState === 'complete')"
        "  document.getElementById('" + name + "').focus();"
        "else"
        "  domAutomationController.send(false);",
        &result));
    ASSERT_TRUE(result);
  }

  void FocusFirstNameField() {
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        GetRenderViewHost(),
        "if (document.readyState === 'complete')"
        "  document.getElementById('firstname').focus();"
        "else"
        "  domAutomationController.send(false);",
        &result));
    ASSERT_TRUE(result);
  }

  // Simulates a click on the middle of the DOM element with the given |id|.
  void ClickElementWithId(const std::string& id) {
    int x;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        GetRenderViewHost(),
        "var bounds = document.getElementById('" +
            id +
            "').getBoundingClientRect();"
            "domAutomationController.send("
            "    Math.floor(bounds.left + bounds.width / 2));",
        &x));
    int y;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        GetRenderViewHost(),
        "var bounds = document.getElementById('" +
            id +
            "').getBoundingClientRect();"
            "domAutomationController.send("
            "    Math.floor(bounds.top + bounds.height / 2));",
        &y));
    content::SimulateMouseClickAt(GetWebContents(),
                                  0,
                                  blink::WebMouseEvent::ButtonLeft,
                                  gfx::Point(x, y));
  }

  void ClickFirstNameField() {
    ASSERT_NO_FATAL_FAILURE(ClickElementWithId("firstname"));
  }

  // Make a pointless round trip to the renderer, giving the popup a chance to
  // show if it's going to. If it does show, an assert in
  // AutofillManagerTestDelegateImpl will trigger.
  void MakeSurePopupDoesntAppear() {
    int unused;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        GetRenderViewHost(), "domAutomationController.send(42)", &unused));
  }

  void ExpectFilledTestForm() {
    ExpectFieldValue("firstname", "Milton");
    ExpectFieldValue("lastname", "Waddams");
    ExpectFieldValue("address1", "4120 Freidrich Lane");
    ExpectFieldValue("address2", "Basement");
    ExpectFieldValue("city", "Austin");
    ExpectFieldValue("state", "TX");
    ExpectFieldValue("zip", "78744");
    ExpectFieldValue("country", "US");
    ExpectFieldValue("phone", "5125551234");
  }

  void SendKeyToPageAndWait(ui::KeyboardCode key) {
    test_delegate_.Reset();
    content::SimulateKeyPress(
        GetWebContents(), key, false, false, false, false);
    test_delegate_.Wait();
  }

  void PasteStringAndWait(const std::string& pastedata) {
    {
      ui::ScopedClipboardWriter writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
      writer.WriteText(base::ASCIIToUTF16(pastedata));
    }
    test_delegate_.Reset();
    GetWebContents()->Paste();
    test_delegate_.WaitForTextChange();
  }

  bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event) {
    return true;
  }

  void SendKeyToPopupAndWait(ui::KeyboardCode key) {
    // Route popup-targeted key presses via the render view host.
    content::NativeWebKeyboardEvent event;
    event.windowsKeyCode = key;
    event.type = blink::WebKeyboardEvent::RawKeyDown;
    test_delegate_.Reset();
    // Install the key press event sink to ensure that any events that are not
    // handled by the installed callbacks do not end up crashing the test.
    GetRenderViewHost()->GetWidget()->AddKeyPressEventCallback(
        key_press_event_sink_);
    GetRenderViewHost()->GetWidget()->ForwardKeyboardEvent(event);
    test_delegate_.Wait();
    GetRenderViewHost()->GetWidget()->RemoveKeyPressEventCallback(
        key_press_event_sink_);
  }

  // Datalist does not support autofill preview. There is no need to start
  // message loop for Datalist.
  void SendKeyToDataListPopup(ui::KeyboardCode key) {
    // Route popup-targeted key presses via the render view host.
    content::NativeWebKeyboardEvent event;
    event.windowsKeyCode = key;
    event.type = blink::WebKeyboardEvent::RawKeyDown;
    // Install the key press event sink to ensure that any events that are not
    // handled by the installed callbacks do not end up crashing the test.
    GetRenderViewHost()->GetWidget()->AddKeyPressEventCallback(
        key_press_event_sink_);
    GetRenderViewHost()->GetWidget()->ForwardKeyboardEvent(event);
    GetRenderViewHost()->GetWidget()->RemoveKeyPressEventCallback(
        key_press_event_sink_);
  }

  void TryBasicFormFill() {
    FocusFirstNameField();

    // Start filling the first name field with "M" and wait for the popup to be
    // shown.
    SendKeyToPageAndWait(ui::VKEY_M);

    // Press the down arrow to select the suggestion and preview the autofilled
    // form.
    SendKeyToPopupAndWait(ui::VKEY_DOWN);

    // The previewed values should not be accessible to JavaScript.
    ExpectFieldValue("firstname", "M");
    ExpectFieldValue("lastname", std::string());
    ExpectFieldValue("address1", std::string());
    ExpectFieldValue("address2", std::string());
    ExpectFieldValue("city", std::string());
    ExpectFieldValue("state", std::string());
    ExpectFieldValue("zip", std::string());
    ExpectFieldValue("country", std::string());
    ExpectFieldValue("phone", std::string());
    // TODO(isherman): It would be nice to test that the previewed values are
    // displayed: http://crbug.com/57220

    // Press Enter to accept the autofill suggestions.
    SendKeyToPopupAndWait(ui::VKEY_RETURN);

    // The form should be filled.
    ExpectFilledTestForm();
  }

  AutofillManagerTestDelegateImpl* test_delegate() { return &test_delegate_; }

 private:
  AutofillManagerTestDelegateImpl test_delegate_;

  net::TestURLFetcherFactory url_fetcher_factory_;

  // KeyPressEventCallback that serves as a sink to ensure that every key press
  // event the tests create and have the WebContents forward is handled by some
  // key press event callback. It is necessary to have this sinkbecause if no
  // key press event callback handles the event (at least on Mac), a DCHECK
  // ends up going off that the |event| doesn't have an |os_event| associated
  // with it.
  content::RenderWidgetHost::KeyPressEventCallback key_press_event_sink_;

  DISALLOW_COPY_AND_ASSIGN(AutofillInteractiveTest);
};

// Test that basic form fill is working.
// Flakily times out on ChromeOS http://crbug.com/585885
#if defined(OS_CHROMEOS)
#define MAYBE_BasicFormFill DISABLED_BasicFormFill
#else
#define MAYBE_BasicFormFill BasicFormFill
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, MAYBE_BasicFormFill) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Invoke Autofill.
  TryBasicFormFill();
}

// Flaky. See http://crbug.com/516052.
#if defined(OS_CHROMEOS)
#define MAYBE_AutofillViaDownArrow DISABLED_AutofillViaDownArrow
#else
#define MAYBE_AutofillViaDownArrow AutofillViaDownArrow
#endif
// Test that form filling can be initiated by pressing the down arrow.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, MAYBE_AutofillViaDownArrow) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Focus a fillable field.
  FocusFirstNameField();

  // Press the down arrow to initiate Autofill and wait for the popup to be
  // shown.
  SendKeyToPageAndWait(ui::VKEY_DOWN);

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::VKEY_DOWN);

  // Press Enter to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::VKEY_RETURN);

  // The form should be filled.
  ExpectFilledTestForm();
}

// crbug.com/516052
#if defined(OS_CHROMEOS)
#define MAYBE_AutofillSelectViaTab DISABLED_AutofillSelectViaTab
#else
#define MAYBE_AutofillSelectViaTab AutofillSelectViaTab
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, MAYBE_AutofillSelectViaTab) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Focus a fillable field.
  FocusFirstNameField();

  // Press the down arrow to initiate Autofill and wait for the popup to be
  // shown.
  SendKeyToPageAndWait(ui::VKEY_DOWN);

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::VKEY_DOWN);

  // Press tab to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::VKEY_TAB);

  // The form should be filled.
  ExpectFilledTestForm();
}

// crbug.com/516052
#if defined(OS_CHROMEOS)
#define MAYBE_AutofillViaClick DISABLED_AutofillViaClick
#else
#define MAYBE_AutofillViaClick AutofillViaClick
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, MAYBE_AutofillViaClick) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kDataURIPrefix) + kTestFormString)));
  // Focus a fillable field.
  ASSERT_NO_FATAL_FAILURE(FocusFirstNameField());

  // Now click it.
  test_delegate()->Reset();
  ASSERT_NO_FATAL_FAILURE(ClickFirstNameField());
  test_delegate()->Wait();

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::VKEY_DOWN);

  // Press Enter to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::VKEY_RETURN);

  // The form should be filled.
  ExpectFilledTestForm();
}

// Makes sure that the first click does *not* activate the popup.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, DontAutofillForFirstClick) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Click the first name field while it's out of focus, then twiddle our thumbs
  // a bit. If a popup were to show, it would hit the asserts in
  // AutofillManagerTestDelegateImpl while we're wasting time.
  ASSERT_NO_FATAL_FAILURE(ClickFirstNameField());
  ASSERT_NO_FATAL_FAILURE(MakeSurePopupDoesntAppear());

  // The second click should activate the popup since the first click focused
  // the field.
  test_delegate()->Reset();
  ASSERT_NO_FATAL_FAILURE(ClickFirstNameField());
  test_delegate()->Wait();
}

// Makes sure that clicking outside the focused field doesn't activate
// the popup.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, DontAutofillForOutsideClick) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString +
           "<button disabled id='disabled-button'>Cant click this</button>")));

  ASSERT_NO_FATAL_FAILURE(FocusFirstNameField());

  // Clicking a disabled button will generate a mouse event but focus doesn't
  // change. This tests that autofill can handle a mouse event outside a focused
  // input *without* showing the popup.
  ASSERT_NO_FATAL_FAILURE(ClickElementWithId("disabled-button"));
  ASSERT_NO_FATAL_FAILURE(MakeSurePopupDoesntAppear());

  test_delegate()->Reset();
  ASSERT_NO_FATAL_FAILURE(ClickFirstNameField());
  test_delegate()->Wait();
}

// Test that a field is still autofillable after the previously autofilled
// value is deleted.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, OnDeleteValueAfterAutofill) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Invoke and accept the Autofill popup and verify the form was filled.
  FocusFirstNameField();
  SendKeyToPageAndWait(ui::VKEY_M);
  SendKeyToPopupAndWait(ui::VKEY_DOWN);
  SendKeyToPopupAndWait(ui::VKEY_RETURN);
  ExpectFilledTestForm();

  // Delete the value of a filled field.
  ASSERT_TRUE(content::ExecuteScript(
      GetRenderViewHost(),
      "document.getElementById('firstname').value = '';"));
  ExpectFieldValue("firstname", "");

  // Invoke and accept the Autofill popup and verify the field was filled.
  SendKeyToPageAndWait(ui::VKEY_M);
  SendKeyToPopupAndWait(ui::VKEY_DOWN);
  SendKeyToPopupAndWait(ui::VKEY_RETURN);
  ExpectFieldValue("firstname", "Milton");
}

// Test that an input field is not rendered with the yellow autofilled
// background color when choosing an option from the datalist suggestion list.
#if defined(OS_MACOSX)
// Flakily triggers and assert on Mac.
// http://crbug.com/419868
#define MAYBE_OnSelectOptionFromDatalist DISABLED_OnSelectOptionFromDatalist
#else
#define MAYBE_OnSelectOptionFromDatalist OnSelectOptionFromDatalist
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       MAYBE_OnSelectOptionFromDatalist) {
  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(),
      GURL(std::string(kDataURIPrefix) +
           "<form action=\"http://www.example.com/\" method=\"POST\">"
           "  <input list=\"dl\" type=\"search\" id=\"firstname\""
           "         onfocus=\"domAutomationController.send(true)\"><br>"
           "  <datalist id=\"dl\">"
           "  <option value=\"Adam\"></option>"
           "  <option value=\"Bob\"></option>"
           "  <option value=\"Carl\"></option>"
           "  </datalist>"
           "</form>")));
  std::string orginalcolor;
  GetFieldBackgroundColor("firstname", &orginalcolor);

  FocusFirstNameField();
  SendKeyToPageAndWait(ui::VKEY_DOWN);
  SendKeyToDataListPopup(ui::VKEY_DOWN);
  SendKeyToDataListPopup(ui::VKEY_RETURN);
  ExpectFieldValue("firstname", "Adam");
  std::string color;
  GetFieldBackgroundColor("firstname", &color);
  EXPECT_EQ(color, orginalcolor);
}

// Test that a JavaScript oninput event is fired after auto-filling a form.
// Flakily times out on ChromeOS http://crbug.com/585885
#if defined(OS_CHROMEOS)
#define MAYBE_OnInputAfterAutofill DISABLED_OnInputAfterAutofill
#else
#define MAYBE_OnInputAfterAutofill OnInputAfterAutofill
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, MAYBE_OnInputAfterAutofill) {
  CreateTestProfile();

  const char kOnInputScript[] =
      "<script>"
      "focused_fired = false;"
      "unfocused_fired = false;"
      "changed_select_fired = false;"
      "unchanged_select_fired = false;"
      "document.getElementById('firstname').oninput = function() {"
      "  focused_fired = true;"
      "};"
      "document.getElementById('lastname').oninput = function() {"
      "  unfocused_fired = true;"
      "};"
      "document.getElementById('state').oninput = function() {"
      "  changed_select_fired = true;"
      "};"
      "document.getElementById('country').oninput = function() {"
      "  unchanged_select_fired = true;"
      "};"
      "document.getElementById('country').value = 'US';"
      "</script>";

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString + kOnInputScript)));

  // Invoke Autofill.
  FocusFirstNameField();

  // Start filling the first name field with "M" and wait for the popup to be
  // shown.
  SendKeyToPageAndWait(ui::VKEY_M);

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::VKEY_DOWN);

  // Press Enter to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::VKEY_RETURN);

  // The form should be filled.
  ExpectFilledTestForm();

  bool focused_fired = false;
  bool unfocused_fired = false;
  bool changed_select_fired = false;
  bool unchanged_select_fired = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetRenderViewHost(),
      "domAutomationController.send(focused_fired);",
      &focused_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetRenderViewHost(),
      "domAutomationController.send(unfocused_fired);",
      &unfocused_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetRenderViewHost(),
      "domAutomationController.send(changed_select_fired);",
      &changed_select_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetRenderViewHost(),
      "domAutomationController.send(unchanged_select_fired);",
      &unchanged_select_fired));
  EXPECT_TRUE(focused_fired);
  EXPECT_TRUE(unfocused_fired);
  EXPECT_TRUE(changed_select_fired);
  EXPECT_FALSE(unchanged_select_fired);
}

// Test that a JavaScript onchange event is fired after auto-filling a form.
// Flaky on CrOS only.  http://crbug.com/578095
#if defined(OS_CHROMEOS)
#define MAYBE_OnChangeAfterAutofill DISABLED_OnChangeAfterAutofill
#else
#define MAYBE_OnChangeAfterAutofill OnChangeAfterAutofill
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, MAYBE_OnChangeAfterAutofill) {
  CreateTestProfile();

  const char kOnChangeScript[] =
      "<script>"
      "focused_fired = false;"
      "unfocused_fired = false;"
      "changed_select_fired = false;"
      "unchanged_select_fired = false;"
      "document.getElementById('firstname').onchange = function() {"
      "  focused_fired = true;"
      "};"
      "document.getElementById('lastname').onchange = function() {"
      "  unfocused_fired = true;"
      "};"
      "document.getElementById('state').onchange = function() {"
      "  changed_select_fired = true;"
      "};"
      "document.getElementById('country').onchange = function() {"
      "  unchanged_select_fired = true;"
      "};"
      "document.getElementById('country').value = 'US';"
      "</script>";

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString + kOnChangeScript)));

  // Invoke Autofill.
  FocusFirstNameField();

  // Start filling the first name field with "M" and wait for the popup to be
  // shown.
  SendKeyToPageAndWait(ui::VKEY_M);

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::VKEY_DOWN);

  // Press Enter to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::VKEY_RETURN);

  // The form should be filled.
  ExpectFilledTestForm();

  bool focused_fired = false;
  bool unfocused_fired = false;
  bool changed_select_fired = false;
  bool unchanged_select_fired = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetRenderViewHost(),
      "domAutomationController.send(focused_fired);",
      &focused_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetRenderViewHost(),
      "domAutomationController.send(unfocused_fired);",
      &unfocused_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetRenderViewHost(),
      "domAutomationController.send(changed_select_fired);",
      &changed_select_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetRenderViewHost(),
      "domAutomationController.send(unchanged_select_fired);",
      &unchanged_select_fired));
  EXPECT_TRUE(focused_fired);
  EXPECT_TRUE(unfocused_fired);
  EXPECT_TRUE(changed_select_fired);
  EXPECT_FALSE(unchanged_select_fired);
}

// Flakily times out on ChromeOS http://crbug.com/585885
#if defined(OS_CHROMEOS)
#define MAYBE_InputFiresBeforeChange DISABLED_InputFiresBeforeChange
#else
#define MAYBE_InputFiresBeforeChange InputFiresBeforeChange
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, MAYBE_InputFiresBeforeChange) {
  CreateTestProfile();

  const char kInputFiresBeforeChangeScript[] =
      "<script>"
      "inputElementEvents = [];"
      "function recordInputElementEvent(e) {"
      "  if (e.target.tagName != 'INPUT') throw 'only <input> tags allowed';"
      "  inputElementEvents.push(e.type);"
      "}"
      "selectElementEvents = [];"
      "function recordSelectElementEvent(e) {"
      "  if (e.target.tagName != 'SELECT') throw 'only <select> tags allowed';"
      "  selectElementEvents.push(e.type);"
      "}"
      "document.getElementById('lastname').oninput = recordInputElementEvent;"
      "document.getElementById('lastname').onchange = recordInputElementEvent;"
      "document.getElementById('country').oninput = recordSelectElementEvent;"
      "document.getElementById('country').onchange = recordSelectElementEvent;"
      "</script>";

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString +
           kInputFiresBeforeChangeScript)));

  // Invoke and accept the Autofill popup and verify the form was filled.
  FocusFirstNameField();
  SendKeyToPageAndWait(ui::VKEY_M);
  SendKeyToPopupAndWait(ui::VKEY_DOWN);
  SendKeyToPopupAndWait(ui::VKEY_RETURN);
  ExpectFilledTestForm();

  int num_input_element_events = -1;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      GetRenderViewHost(),
      "domAutomationController.send(inputElementEvents.length);",
      &num_input_element_events));
  EXPECT_EQ(2, num_input_element_events);

  std::vector<std::string> input_element_events;
  input_element_events.resize(2);

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetRenderViewHost(),
      "domAutomationController.send(inputElementEvents[0]);",
      &input_element_events[0]));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetRenderViewHost(),
      "domAutomationController.send(inputElementEvents[1]);",
      &input_element_events[1]));

  EXPECT_EQ("input", input_element_events[0]);
  EXPECT_EQ("change", input_element_events[1]);

  int num_select_element_events = -1;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      GetRenderViewHost(),
      "domAutomationController.send(selectElementEvents.length);",
      &num_select_element_events));
  EXPECT_EQ(2, num_select_element_events);

  std::vector<std::string> select_element_events;
  select_element_events.resize(2);

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetRenderViewHost(),
      "domAutomationController.send(selectElementEvents[0]);",
      &select_element_events[0]));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetRenderViewHost(),
      "domAutomationController.send(selectElementEvents[1]);",
      &select_element_events[1]));

  EXPECT_EQ("input", select_element_events[0]);
  EXPECT_EQ("change", select_element_events[1]);
}

// Test that we can autofill forms distinguished only by their |id| attribute.
// Flaky on CrOS only.  http://crbug.com/578095
#if defined(OS_CHROMEOS)
#define MAYBE_AutofillFormsDistinguishedById DISABLED_AutofillFormsDistinguishedById
#else
#define MAYBE_AutofillFormsDistinguishedById AutofillFormsDistinguishedById
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       MAYBE_AutofillFormsDistinguishedById) {
  CreateTestProfile();

  // Load the test page.
  const std::string kURL =
      std::string(kDataURIPrefix) + kTestFormString +
      "<script>"
      "var mainForm = document.forms[0];"
      "mainForm.id = 'mainForm';"
      "var newForm = document.createElement('form');"
      "newForm.action = mainForm.action;"
      "newForm.method = mainForm.method;"
      "newForm.id = 'newForm';"
      "mainForm.parentNode.insertBefore(newForm, mainForm);"
      "</script>";
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), GURL(kURL)));

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test that we properly autofill forms with repeated fields.
// In the wild, the repeated fields are typically either email fields
// (duplicated for "confirmation"); or variants that are hot-swapped via
// JavaScript, with only one actually visible at any given time.
// Flakily times out on ChromeOS http://crbug.com/585885
#if defined(OS_CHROMEOS)
#define MAYBE_AutofillFormWithRepeatedField \
  DISABLED_AutofillFormWithRepeatedField
#else
#define MAYBE_AutofillFormWithRepeatedField AutofillFormWithRepeatedField
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       MAYBE_AutofillFormWithRepeatedField) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) +
           "<form action=\"http://www.example.com/\" method=\"POST\">"
           "<label for=\"firstname\">First name:</label>"
           " <input type=\"text\" id=\"firstname\""
           "        onfocus=\"domAutomationController.send(true)\"><br>"
           "<label for=\"lastname\">Last name:</label>"
           " <input type=\"text\" id=\"lastname\"><br>"
           "<label for=\"address1\">Address line 1:</label>"
           " <input type=\"text\" id=\"address1\"><br>"
           "<label for=\"address2\">Address line 2:</label>"
           " <input type=\"text\" id=\"address2\"><br>"
           "<label for=\"city\">City:</label>"
           " <input type=\"text\" id=\"city\"><br>"
           "<label for=\"state\">State:</label>"
           " <select id=\"state\">"
           " <option value=\"\" selected=\"yes\">--</option>"
           " <option value=\"CA\">California</option>"
           " <option value=\"TX\">Texas</option>"
           " </select><br>"
           "<label for=\"state_freeform\" style=\"display:none\">State:</label>"
           " <input type=\"text\" id=\"state_freeform\""
           "        style=\"display:none\"><br>"
           "<label for=\"zip\">ZIP code:</label>"
           " <input type=\"text\" id=\"zip\"><br>"
           "<label for=\"country\">Country:</label>"
           " <select id=\"country\">"
           " <option value=\"\" selected=\"yes\">--</option>"
           " <option value=\"CA\">Canada</option>"
           " <option value=\"US\">United States</option>"
           " </select><br>"
           "<label for=\"phone\">Phone number:</label>"
           " <input type=\"text\" id=\"phone\"><br>"
           "</form>")));

  // Invoke Autofill.
  TryBasicFormFill();
  ExpectFieldValue("state_freeform", std::string());
}

// Test that we properly autofill forms with non-autofillable fields.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       AutofillFormWithNonAutofillableField) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) +
           "<form action=\"http://www.example.com/\" method=\"POST\">"
           "<label for=\"firstname\">First name:</label>"
           " <input type=\"text\" id=\"firstname\""
           "        onfocus=\"domAutomationController.send(true)\"><br>"
           "<label for=\"middlename\">Middle name:</label>"
           " <input type=\"text\" id=\"middlename\" autocomplete=\"off\" /><br>"
           "<label for=\"lastname\">Last name:</label>"
           " <input type=\"text\" id=\"lastname\"><br>"
           "<label for=\"address1\">Address line 1:</label>"
           " <input type=\"text\" id=\"address1\"><br>"
           "<label for=\"address2\">Address line 2:</label>"
           " <input type=\"text\" id=\"address2\"><br>"
           "<label for=\"city\">City:</label>"
           " <input type=\"text\" id=\"city\"><br>"
           "<label for=\"state\">State:</label>"
           " <select id=\"state\">"
           " <option value=\"\" selected=\"yes\">--</option>"
           " <option value=\"CA\">California</option>"
           " <option value=\"TX\">Texas</option>"
           " </select><br>"
           "<label for=\"zip\">ZIP code:</label>"
           " <input type=\"text\" id=\"zip\"><br>"
           "<label for=\"country\">Country:</label>"
           " <select id=\"country\">"
           " <option value=\"\" selected=\"yes\">--</option>"
           " <option value=\"CA\">Canada</option>"
           " <option value=\"US\">United States</option>"
           " </select><br>"
           "<label for=\"phone\">Phone number:</label>"
           " <input type=\"text\" id=\"phone\"><br>"
           "</form>")));

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test that we can Autofill dynamically generated forms.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, DynamicFormFill) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) +
           "<form id=\"form\" action=\"http://www.example.com/\""
           "      method=\"POST\"></form>"
           "<script>"
           "function AddElement(name, label) {"
           "  var form = document.getElementById('form');"
           ""
           "  var label_text = document.createTextNode(label);"
           "  var label_element = document.createElement('label');"
           "  label_element.setAttribute('for', name);"
           "  label_element.appendChild(label_text);"
           "  form.appendChild(label_element);"
           ""
           "  if (name === 'state' || name === 'country') {"
           "    var select_element = document.createElement('select');"
           "    select_element.setAttribute('id', name);"
           "    select_element.setAttribute('name', name);"
           ""
           "    /* Add an empty selected option. */"
           "    var default_option = new Option('--', '', true);"
           "    select_element.appendChild(default_option);"
           ""
           "    /* Add the other options. */"
           "    if (name == 'state') {"
           "      var option1 = new Option('California', 'CA');"
           "      select_element.appendChild(option1);"
           "      var option2 = new Option('Texas', 'TX');"
           "      select_element.appendChild(option2);"
           "    } else {"
           "      var option1 = new Option('Canada', 'CA');"
           "      select_element.appendChild(option1);"
           "      var option2 = new Option('United States', 'US');"
           "      select_element.appendChild(option2);"
           "    }"
           ""
           "    form.appendChild(select_element);"
           "  } else {"
           "    var input_element = document.createElement('input');"
           "    input_element.setAttribute('id', name);"
           "    input_element.setAttribute('name', name);"
           ""
           "    /* Add the onfocus listener to the 'firstname' field. */"
           "    if (name === 'firstname') {"
           "      input_element.onfocus = function() {"
           "        domAutomationController.send(true);"
           "      };"
           "    }"
           ""
           "    form.appendChild(input_element);"
           "  }"
           ""
           "  form.appendChild(document.createElement('br'));"
           "};"
           ""
           "function BuildForm() {"
           "  var elements = ["
           "    ['firstname', 'First name:'],"
           "    ['lastname', 'Last name:'],"
           "    ['address1', 'Address line 1:'],"
           "    ['address2', 'Address line 2:'],"
           "    ['city', 'City:'],"
           "    ['state', 'State:'],"
           "    ['zip', 'ZIP code:'],"
           "    ['country', 'Country:'],"
           "    ['phone', 'Phone number:'],"
           "  ];"
           ""
           "  for (var i = 0; i < elements.length; i++) {"
           "    var name = elements[i][0];"
           "    var label = elements[i][1];"
           "    AddElement(name, label);"
           "  }"
           "};"
           "</script>")));

  // Dynamically construct the form.
  ASSERT_TRUE(content::ExecuteScript(GetRenderViewHost(), "BuildForm();"));

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test that form filling works after reloading the current page.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillAfterReload) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Reload the page.
  content::WebContents* web_contents = GetWebContents();
  web_contents->GetController().Reload(false);
  content::WaitForLoadStop(web_contents);

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test fails on Linux ASAN, see http://crbug.com/532737
#if defined(ADDRESS_SANITIZER)
#define MAYBE_AutofillAfterTranslate DISABLED_AutofillAfterTranslate
#else
#define MAYBE_AutofillAfterTranslate AutofillAfterTranslate
#endif  // ADDRESS_SANITIZER
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, MAYBE_AutofillAfterTranslate) {
// TODO(groby): Remove once the bubble is enabled by default everywhere.
// http://crbug.com/507442
#if defined(OS_MACOSX)
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableTranslateNewUX);
#endif
  ASSERT_TRUE(TranslateService::IsTranslateBubbleEnabled());

  translate::TranslateManager::SetIgnoreMissingKeyForTesting(true);

  CreateTestProfile();

  GURL url(std::string(kDataURIPrefix) +
               "<form action=\"http://www.example.com/\" method=\"POST\">"
               "<label for=\"fn\">なまえ</label>"
               " <input type=\"text\" id=\"fn\""
               "        onfocus=\"domAutomationController.send(true)\""
               "><br>"
               "<label for=\"ln\">みょうじ</label>"
               " <input type=\"text\" id=\"ln\"><br>"
               "<label for=\"a1\">Address line 1:</label>"
               " <input type=\"text\" id=\"a1\"><br>"
               "<label for=\"a2\">Address line 2:</label>"
               " <input type=\"text\" id=\"a2\"><br>"
               "<label for=\"ci\">City:</label>"
               " <input type=\"text\" id=\"ci\"><br>"
               "<label for=\"st\">State:</label>"
               " <select id=\"st\">"
               " <option value=\"\" selected=\"yes\">--</option>"
               " <option value=\"CA\">California</option>"
               " <option value=\"TX\">Texas</option>"
               " </select><br>"
               "<label for=\"z\">ZIP code:</label>"
               " <input type=\"text\" id=\"z\"><br>"
               "<label for=\"co\">Country:</label>"
               " <select id=\"co\">"
               " <option value=\"\" selected=\"yes\">--</option>"
               " <option value=\"CA\">Canada</option>"
               " <option value=\"US\">United States</option>"
               " </select><br>"
               "<label for=\"ph\">Phone number:</label>"
               " <input type=\"text\" id=\"ph\"><br>"
               "</form>"
               // Add additional Japanese characters to ensure the translate bar
               // will appear.
               "我々は重要な、興味深いものになるが、時折状況が発生するため苦労や痛みは"
               "彼にいくつかの素晴らしいを調達することができます。それから、いくつかの利");

  // Set up an observer to be able to wait for the bubble to be shown.
  content::Source<content::WebContents> source(GetWebContents());
  content::WindowedNotificationObserver language_detected_signal(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED, source);

  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), url));

  // Wait for the translate bubble to appear.
  language_detected_signal.Wait();

  // Verify current translate step.
  const TranslateBubbleModel* model =
      translate::test_utils::GetCurrentModel(browser());
  ASSERT_NE(nullptr, model);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            model->GetViewState());

  translate::test_utils::PressTranslate(browser());

  // Wait for translation.
  content::WindowedNotificationObserver translation_observer(
      chrome::NOTIFICATION_PAGE_TRANSLATED,
      content::NotificationService::AllSources());

  // Simulate the translate script being retrieved.
  // Pass fake google.translate lib as the translate script.
  SimulateURLFetch(true);

  // Simulate the render notifying the translation has been done.
  translation_observer.Wait();

  TryBasicFormFill();
}

// Test phone fields parse correctly from a given profile.
// The high level key presses execute the following: Select the first text
// field, invoke the autofill popup list, select the first profile within the
// list, and commit to the profile to populate the form.
// Flakily times out on windows. http://crbug.com/390564
// Flaky on the official cros-trunk crbug.com/516052
#if defined(OS_WIN) || defined(OFFICIAL_BUILD)
#define MAYBE_ComparePhoneNumbers DISABLED_ComparePhoneNumbers
#else
#define MAYBE_ComparePhoneNumbers ComparePhoneNumbers
#endif  // defined(OS_WIN) || defined(OFFICIAL_BUILD)
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, MAYBE_ComparePhoneNumbers) {
  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Bob"));
  profile.SetRawInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 H St."));
  profile.SetRawInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("San Jose"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("95110"));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("1-408-555-4567"));
  SetTestProfile(browser(), profile);

  GURL url = embedded_test_server()->GetURL("/autofill/form_phones.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  ExpectFieldValue("NAME_FIRST", "Bob");
  ExpectFieldValue("NAME_LAST", "Smith");
  ExpectFieldValue("ADDRESS_HOME_LINE1", "1234 H St.");
  ExpectFieldValue("ADDRESS_HOME_CITY", "San Jose");
  ExpectFieldValue("ADDRESS_HOME_STATE", "CA");
  ExpectFieldValue("ADDRESS_HOME_ZIP", "95110");
  ExpectFieldValue("PHONE_HOME_WHOLE_NUMBER", "14085554567");
  ExpectFieldValue("PHONE_HOME_CITY_CODE-1", "408");
  ExpectFieldValue("PHONE_HOME_CITY_CODE-2", "408");
  ExpectFieldValue("PHONE_HOME_NUMBER", "5554567");
  ExpectFieldValue("PHONE_HOME_NUMBER_3-1", "555");
  ExpectFieldValue("PHONE_HOME_NUMBER_3-2", "555");
  ExpectFieldValue("PHONE_HOME_NUMBER_4-1", "4567");
  ExpectFieldValue("PHONE_HOME_NUMBER_4-2", "4567");
  ExpectFieldValue("PHONE_HOME_EXT-1", std::string());
  ExpectFieldValue("PHONE_HOME_EXT-2", std::string());
  ExpectFieldValue("PHONE_HOME_COUNTRY_CODE-1", "1");
}

// Test that Autofill does not fill in read-only fields.
// Flaky on the official cros-trunk. crbug.com/516052
// Also flaky on ChromiumOS generally. crbug.com/585885
#if defined(OFFICIAL_BUILD) || defined(OS_CHROMEOS)
#define MAYBE_NoAutofillForReadOnlyFields DISABLED_NoAutofillForReadOnlyFields
#else
#define MAYBE_NoAutofillForReadOnlyFields NoAutofillForReadOnlyFields
#endif  // defined(OFFICIAL_BUILD) || defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       MAYBE_NoAutofillForReadOnlyFields) {
  std::string addr_line1("1234 H St.");

  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Bob"));
  profile.SetRawInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("bsmith@gmail.com"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16(addr_line1));
  profile.SetRawInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("San Jose"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("95110"));
  profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Company X"));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("408-871-4567"));
  SetTestProfile(browser(), profile);

  GURL url =
      embedded_test_server()->GetURL("/autofill/read_only_field_test.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("firstname");

  ExpectFieldValue("email", std::string());
  ExpectFieldValue("address", addr_line1);
}

// Test form is fillable from a profile after form was reset.
// Steps:
//   1. Fill form using a saved profile.
//   2. Reset the form.
//   3. Fill form using a saved profile.
// Flakily times out: http://crbug.com/270341
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, DISABLED_FormFillableOnReset) {
  CreateTestProfile();

  GURL url =
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  ASSERT_TRUE(content::ExecuteScript(
       GetWebContents(), "document.getElementById('testform').reset()"));

  PopulateForm("NAME_FIRST");

  ExpectFieldValue("NAME_FIRST", "Milton");
  ExpectFieldValue("NAME_LAST", "Waddams");
  ExpectFieldValue("EMAIL_ADDRESS", "red.swingline@initech.com");
  ExpectFieldValue("ADDRESS_HOME_LINE1", "4120 Freidrich Lane");
  ExpectFieldValue("ADDRESS_HOME_CITY", "Austin");
  ExpectFieldValue("ADDRESS_HOME_STATE", "Texas");
  ExpectFieldValue("ADDRESS_HOME_ZIP", "78744");
  ExpectFieldValue("ADDRESS_HOME_COUNTRY", "United States");
  ExpectFieldValue("PHONE_HOME_WHOLE_NUMBER", "5125551234");
}

// Test Autofill distinguishes a middle initial in a name.
// Flakily times out: http://crbug.com/270341
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       DISABLED_DistinguishMiddleInitialWithinName) {
  CreateTestProfile();

  GURL url =
      embedded_test_server()->GetURL("/autofill/autofill_middleinit_form.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  ExpectFieldValue("NAME_MIDDLE", "C");
}

// Test forms with multiple email addresses are filled properly.
// Entire form should be filled with one user gesture.
// Flakily times out: http://crbug.com/270341
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       DISABLED_MultipleEmailFilledByOneUserGesture) {
  std::string email("bsmith@gmail.com");

  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Bob"));
  profile.SetRawInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16(email));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("4088714567"));
  SetTestProfile(browser(), profile);

  GURL url = embedded_test_server()->GetURL(
      "/autofill/autofill_confirmemail_form.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  ExpectFieldValue("EMAIL_CONFIRM", email);
  // TODO(isherman): verify entire form.
}

// Test latency time on form submit with lots of stored Autofill profiles.
// This test verifies when a profile is selected from the Autofill dictionary
// that consists of thousands of profiles, the form does not hang after being
// submitted.
// Flakily times out: http://crbug.com/281527
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       DISABLED_FormFillLatencyAfterSubmit) {
  std::vector<std::string> cities;
  cities.push_back("San Jose");
  cities.push_back("San Francisco");
  cities.push_back("Sacramento");
  cities.push_back("Los Angeles");

  std::vector<std::string> streets;
  streets.push_back("St");
  streets.push_back("Ave");
  streets.push_back("Ln");
  streets.push_back("Ct");

  const int kNumProfiles = 1500;
  base::Time start_time = base::Time::Now();
  std::vector<AutofillProfile> profiles;
  for (int i = 0; i < kNumProfiles; i++) {
    AutofillProfile profile;
    base::string16 name(base::IntToString16(i));
    base::string16 email(name + ASCIIToUTF16("@example.com"));
    base::string16 street = ASCIIToUTF16(
        base::IntToString(base::RandInt(0, 10000)) + " " +
        streets[base::RandInt(0, streets.size() - 1)]);
    base::string16 city =
        ASCIIToUTF16(cities[base::RandInt(0, cities.size() - 1)]);
    base::string16 zip(base::IntToString16(base::RandInt(0, 10000)));
    profile.SetRawInfo(NAME_FIRST, name);
    profile.SetRawInfo(EMAIL_ADDRESS, email);
    profile.SetRawInfo(ADDRESS_HOME_LINE1, street);
    profile.SetRawInfo(ADDRESS_HOME_CITY, city);
    profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
    profile.SetRawInfo(ADDRESS_HOME_ZIP, zip);
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
    profiles.push_back(profile);
  }
  SetTestProfiles(browser(), &profiles);
  // TODO(isherman): once we're sure this test doesn't timeout on any bots, this
  // can be removd.
  LOG(INFO) << "Created " << kNumProfiles << " profiles in " <<
               (base::Time::Now() - start_time).InSeconds() << " seconds.";

  GURL url = embedded_test_server()->GetURL(
      "/autofill/latency_after_submit_test.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  content::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &GetWebContents()->GetController()));

  ASSERT_TRUE(content::ExecuteScript(
      GetRenderViewHost(),
      "document.getElementById('testform').submit();"));
  // This will ensure the test didn't hang.
  load_stop_observer.Wait();
}

// Test that Chrome doesn't crash when autocomplete is disabled while the user
// is interacting with the form.  This is a regression test for
// http://crbug.com/160476
// Flakily times out on ChromeOS http://crbug.com/585885
#if defined(OS_CHROMEOS)
#define MAYBE_DisableAutocompleteWhileFilling \
  DISABLED_DisableAutocompleteWhileFilling
#else
#define MAYBE_DisableAutocompleteWhileFilling DisableAutocompleteWhileFilling
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       MAYBE_DisableAutocompleteWhileFilling) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Invoke Autofill: Start filling the first name field with "M" and wait for
  // the popup to be shown.
  FocusFirstNameField();
  SendKeyToPageAndWait(ui::VKEY_M);

  // Now that the popup with suggestions is showing, disable autocomplete for
  // the active field.
  ASSERT_TRUE(content::ExecuteScript(
      GetRenderViewHost(),
      "document.querySelector('input').autocomplete = 'off';"));

  // Press the down arrow to select the suggestion and attempt to preview the
  // autofilled form.
  SendKeyToPopupAndWait(ui::VKEY_DOWN);
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       PastedPasswordIsSaved) {
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestPasswordFormString)));
  ASSERT_TRUE(content::ExecuteScript(
      GetRenderViewHost(),
      "document.getElementById('user').value = 'user';"));
  FocusFieldByName("password");
  PasteStringAndWait("foobar");
}

}  // namespace autofill
