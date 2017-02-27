// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_autofill_agent.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/renderer/autofill/fake_content_password_manager_driver.h"
#include "chrome/renderer/autofill/fake_password_manager_client.h"
#include "chrome/renderer/autofill/password_generation_test_utils.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/autofill/content/renderer/test_password_generation_agent.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/common/associated_interface_provider.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/events/keycodes/keyboard_codes.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebString;
using blink::WebView;

namespace {

const int kPasswordFillFormDataId = 1234;

// The name of the username/password element in the form.
const char kUsernameName[] = "username";
const char kPasswordName[] = "password";
const char kEmailName[] = "email";
const char kCreditCardOwnerName[] = "creditcardowner";
const char kCreditCardNumberName[] = "creditcardnumber";
const char kCreditCardVerificationName[] = "cvc";

const char kAliceUsername[] = "alice";
const char kAlicePassword[] = "password";
const char kBobUsername[] = "bob";
const char kBobPassword[] = "secret";
const char kCarolUsername[] = "Carol";
const char kCarolPassword[] = "test";
const char kCarolAlternateUsername[] = "RealCarolUsername";

const char kFormHTML[] =
    "<FORM id='LoginTestForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='random_field'/>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='password' id='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kVisibleFormWithNoUsernameHTML[] =
    "<head> <style> form {display: inline;} </style> </head>"
    "<body>"
    "  <form name='LoginTestForm' action='http://www.bidule.com'>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

const char kEmptyFormHTML[] =
    "<head> <style> form {display: inline;} </style> </head>"
    "<body> <form> </form> </body>";

const char kNonVisibleFormHTML[] =
    "<head> <style> form {display: none;} </style> </head>"
    "<body>"
    "  <form>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

const char kSignupFormHTML[] =
    "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='random_info'/>"
    "  <INPUT type='password' id='new_password'/>"
    "  <INPUT type='password' id='confirm_password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kEmptyWebpage[] =
    "<html>"
    "   <head>"
    "   </head>"
    "   <body>"
    "   </body>"
    "</html>";

const char kRedirectionWebpage[] =
    "<html>"
    "   <head>"
    "       <meta http-equiv='Content-Type' content='text/html'>"
    "       <title>Redirection page</title>"
    "       <script></script>"
    "   </head>"
    "   <body>"
    "       <script type='text/javascript'>"
    "         function test(){}"
    "       </script>"
    "   </body>"
    "</html>";

const char kSimpleWebpage[] =
    "<html>"
    "   <head>"
    "       <meta charset='utf-8' />"
    "       <title>Title</title>"
    "   </head>"
    "   <body>"
    "       <form name='LoginTestForm'>"
    "           <input type='text' id='username'/>"
    "           <input type='password' id='password'/>"
    "           <input type='submit' value='Login'/>"
    "       </form>"
    "   </body>"
    "</html>";

const char kWebpageWithDynamicContent[] =
    "<html>"
    "   <head>"
    "       <meta charset='utf-8' />"
    "       <title>Title</title>"
    "   </head>"
    "   <body>"
    "       <script type='text/javascript'>"
    "           function addParagraph() {"
    "             var p = document.createElement('p');"
    "             document.body.appendChild(p);"
    "            }"
    "           window.onload = addParagraph;"
    "       </script>"
    "   </body>"
    "</html>";

const char kJavaScriptClick[] =
    "var event = new MouseEvent('click', {"
    "   'view': window,"
    "   'bubbles': true,"
    "   'cancelable': true"
    "});"
    "var form = document.getElementById('myform1');"
    "form.dispatchEvent(event);"
    "console.log('clicked!');";

const char kOnChangeDetectionScript[] =
    "<script>"
    "  usernameOnchangeCalled = false;"
    "  passwordOnchangeCalled = false;"
    "  document.getElementById('username').onchange = function() {"
    "    usernameOnchangeCalled = true;"
    "  };"
    "  document.getElementById('password').onchange = function() {"
    "    passwordOnchangeCalled = true;"
    "  };"
    "</script>";

const char kFormHTMLWithTwoTextFields[] =
    "<FORM name='LoginTestForm' id='LoginTestForm' "
    "action='http://www.bidule.com'>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='text' id='email'/>"
    "  <INPUT type='password' id='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kPasswordChangeFormHTML[] =
    "<FORM name='ChangeWithUsernameForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='password' id='password'/>"
    "  <INPUT type='password' id='newpassword'/>"
    "  <INPUT type='password' id='confirmpassword'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kCreditCardFormHTML[] =
    "<FORM name='ChangeWithUsernameForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='creditcardowner'/>"
    "  <INPUT type='text' id='creditcardnumber'/>"
    "  <INPUT type='password' id='cvc'/>"
    "  <INPUT type='submit' value='Submit'/>"
    "</FORM>";

const char kNoFormHTML[] =
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='password' id='password'/>";

const char kTwoNoUsernameFormsHTML[] =
    "<FORM name='form1' action='http://www.bidule.com'>"
    "  <INPUT type='password' id='password1' name='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>"
    "<FORM name='form2' action='http://www.bidule.com'>"
    "  <INPUT type='password' id='password2' name='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

// Sets the "readonly" attribute of |element| to the value given by |read_only|.
void SetElementReadOnly(WebInputElement& element, bool read_only) {
  element.setAttribute(WebString::fromUTF8("readonly"),
                       read_only ? WebString::fromUTF8("true") : WebString());
}

enum PasswordFormSourceType {
  PasswordFormSubmitted,
  PasswordFormInPageNavigation,
};

}  // namespace

namespace autofill {

class PasswordAutofillAgentTest : public ChromeRenderViewTest {
 public:
  PasswordAutofillAgentTest() {
  }

  // Simulates the fill password form message being sent to the renderer.
  // We use that so we don't have to make RenderView::OnFillPasswordForm()
  // protected.
  void SimulateOnFillPasswordForm(
      const PasswordFormFillData& fill_data) {
    password_autofill_agent_->FillPasswordForm(kPasswordFillFormDataId,
                                               fill_data);
  }

  // Simulates the show initial password account suggestions message being sent
  // to the renderer.
  void SimulateOnShowInitialPasswordAccountSuggestions(
      const PasswordFormFillData& fill_data) {
    autofill_agent_->ShowInitialPasswordAccountSuggestions(
        kPasswordFillFormDataId, fill_data);
  }

  void SendVisiblePasswordForms() {
    static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
        ->DidFinishLoad();
  }

  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // Add a preferred login and an additional login to the FillData.
    username1_ = ASCIIToUTF16(kAliceUsername);
    password1_ = ASCIIToUTF16(kAlicePassword);
    username2_ = ASCIIToUTF16(kBobUsername);
    password2_ = ASCIIToUTF16(kBobPassword);
    username3_ = ASCIIToUTF16(kCarolUsername);
    password3_ = ASCIIToUTF16(kCarolPassword);
    alternate_username3_ = ASCIIToUTF16(kCarolAlternateUsername);

    FormFieldData username_field;
    username_field.name = ASCIIToUTF16(kUsernameName);
    username_field.value = username1_;
    fill_data_.username_field = username_field;

    FormFieldData password_field;
    password_field.name = ASCIIToUTF16(kPasswordName);
    password_field.value = password1_;
    password_field.form_control_type = "password";
    fill_data_.password_field = password_field;

    PasswordAndRealm password2;
    password2.password = password2_;
    fill_data_.additional_logins[username2_] = password2;
    PasswordAndRealm password3;
    password3.password = password3_;
    fill_data_.additional_logins[username3_] = password3;

    UsernamesCollectionKey key;
    key.username = username3_;
    key.password = password3_;
    key.realm = "google.com";
    fill_data_.other_possible_usernames[key].push_back(alternate_username3_);

    // We need to set the origin so it matches the frame URL and the action so
    // it matches the form action, otherwise we won't autocomplete.
    UpdateOriginForHTML(kFormHTML);
    fill_data_.action = GURL("http://www.bidule.com");

    LoadHTML(kFormHTML);

    // Necessary for SimulateElementClick() to work correctly.
    GetWebWidget()->resize(blink::WebSize(500, 500));
    GetWebWidget()->setFocus(true);

    // Now retrieve the input elements so the test can access them.
    UpdateUsernameAndPasswordElements();
  }

  void TearDown() override {
    username_element_.reset();
    password_element_.reset();
    ChromeRenderViewTest::TearDown();
  }

  void RegisterMainFrameRemoteInterfaces() override {
    // We only use the fake driver for main frame
    // because our test cases only involve the main frame.
    service_manager::InterfaceProvider* remote_interfaces =
        view_->GetMainRenderFrame()->GetRemoteInterfaces();
    service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
    test_api.SetBinderForName(
        mojom::PasswordManagerDriver::Name_,
        base::Bind(&PasswordAutofillAgentTest::BindPasswordManagerDriver,
                   base::Unretained(this)));

    // Because the test cases only involve the main frame in this test,
    // the fake password client is only used for the main frame.
    content::AssociatedInterfaceProvider* remote_associated_interfaces =
        view_->GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
    remote_associated_interfaces->OverrideBinderForTesting(
        mojom::PasswordManagerClient::Name_,
        base::Bind(&PasswordAutofillAgentTest::BindPasswordManagerClient,
                   base::Unretained(this)));
  }

  void SetFillOnAccountSelect() {
    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kFillOnAccountSelect);
  }

  void EnableShowAutofillSignatures() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kShowAutofillSignatures);
  }

  void UpdateOriginForHTML(const std::string& html) {
    std::string origin = "data:text/html;charset=utf-8," + html;
    fill_data_.origin = GURL(origin);
  }

  void UpdateUsernameAndPasswordElements() {
    WebDocument document = GetMainFrame()->document();
    WebElement element =
        document.getElementById(WebString::fromUTF8(kUsernameName));
    ASSERT_FALSE(element.isNull());
    username_element_ = element.to<blink::WebInputElement>();
    element = document.getElementById(WebString::fromUTF8(kPasswordName));
    ASSERT_FALSE(element.isNull());
    password_element_ = element.to<blink::WebInputElement>();
  }

  blink::WebInputElement GetInputElementByID(const std::string& id) {
    WebDocument document = GetMainFrame()->document();
    WebElement element =
        document.getElementById(WebString::fromUTF8(id.c_str()));
    return element.to<blink::WebInputElement>();
  }

  void ClearUsernameAndPasswordFields() {
    username_element_.setValue("");
    username_element_.setAutofilled(false);
    password_element_.setValue("");
    password_element_.setAutofilled(false);
  }

  void SimulateSuggestionChoice(WebInputElement& username_input) {
    base::string16 username(base::ASCIIToUTF16(kAliceUsername));
    base::string16 password(base::ASCIIToUTF16(kAlicePassword));
    SimulateSuggestionChoiceOfUsernameAndPassword(username_input, username,
                                                  password);
  }

  void SimulateSuggestionChoiceOfUsernameAndPassword(
      WebInputElement& input,
      const base::string16& username,
      const base::string16& password) {
    // This call is necessary to setup the autofill agent appropriate for the
    // user selection; simulates the menu actually popping up.
    static_cast<autofill::PageClickListener*>(autofill_agent_)
        ->FormControlElementClicked(input, false);

    autofill_agent_->FillPasswordSuggestion(username, password);
  }

  void SimulateUsernameChange(const std::string& username) {
    SimulateUserInputChangeForElement(&username_element_, username);
  }

  void SimulatePasswordChange(const std::string& password) {
    SimulateUserInputChangeForElement(&password_element_, password);
  }

  void CheckTextFieldsStateForElements(const WebInputElement& username_element,
                                       const std::string& username,
                                       bool username_autofilled,
                                       const WebInputElement& password_element,
                                       const std::string& password,
                                       bool password_autofilled,
                                       bool checkSuggestedValue) {
    EXPECT_EQ(username, username_element.value().utf8());
    EXPECT_EQ(username_autofilled, username_element.isAutofilled());
    EXPECT_EQ(password, checkSuggestedValue
                            ? password_element.suggestedValue().utf8()
                            : password_element.value().utf8())
        << "checkSuggestedValue == " << checkSuggestedValue;
    EXPECT_EQ(password_autofilled, password_element.isAutofilled());
  }

  // Checks the DOM-accessible value of the username element and the
  // *suggested* value of the password element.
  void CheckTextFieldsState(const std::string& username,
                            bool username_autofilled,
                            const std::string& password,
                            bool password_autofilled) {
    CheckTextFieldsStateForElements(username_element_,
                                    username,
                                    username_autofilled,
                                    password_element_,
                                    password,
                                    password_autofilled,
                                    true);
  }

  // Checks the DOM-accessible value of the username element and the
  // DOM-accessible value of the password element.
  void CheckTextFieldsDOMState(const std::string& username,
                               bool username_autofilled,
                               const std::string& password,
                               bool password_autofilled) {
    CheckTextFieldsStateForElements(username_element_,
                                    username,
                                    username_autofilled,
                                    password_element_,
                                    password,
                                    password_autofilled,
                                    false);
  }

  void CheckUsernameSelection(int start, int end) {
    EXPECT_EQ(start, username_element_.selectionStart());
    EXPECT_EQ(end, username_element_.selectionEnd());
  }

  // Checks the message sent to PasswordAutofillManager to build the suggestion
  // list. |username| is the expected username field value, and |show_all| is
  // the expected flag for the PasswordAutofillManager, whether to show all
  // suggestions, or only those starting with |username|.
  void CheckSuggestions(const std::string& username, bool show_all) {
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(fake_driver_.called_show_pw_suggestions());
    EXPECT_EQ(kPasswordFillFormDataId, fake_driver_.show_pw_suggestions_key());
    ASSERT_TRUE(static_cast<bool>(fake_driver_.show_pw_suggestions_username()));
    EXPECT_EQ(ASCIIToUTF16(username),
              *(fake_driver_.show_pw_suggestions_username()));
    EXPECT_EQ(show_all,
              static_cast<bool>(fake_driver_.show_pw_suggestions_options() &
                                autofill::SHOW_ALL));

    fake_driver_.reset_show_pw_suggestions();
  }

  bool GetCalledShowPasswordSuggestions() {
    base::RunLoop().RunUntilIdle();
    return fake_driver_.called_show_pw_suggestions();
  }

  void ExpectFormSubmittedWithUsernameAndPasswords(
      const std::string& username_value,
      const std::string& password_value,
      const std::string& new_password_value) {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(fake_driver_.called_password_form_submitted());
    ASSERT_TRUE(static_cast<bool>(fake_driver_.password_form_submitted()));
    const autofill::PasswordForm& form =
        *(fake_driver_.password_form_submitted());
    EXPECT_EQ(ASCIIToUTF16(username_value), form.username_value);
    EXPECT_EQ(ASCIIToUTF16(password_value), form.password_value);
    EXPECT_EQ(ASCIIToUTF16(new_password_value), form.new_password_value);
  }

  void ExpectFieldPropertiesMasks(
      PasswordFormSourceType expected_type,
      const std::map<base::string16, FieldPropertiesMask>&
          expected_properties_masks) {
    base::RunLoop().RunUntilIdle();
    autofill::PasswordForm form;
    if (expected_type == PasswordFormSubmitted) {
      ASSERT_TRUE(fake_driver_.called_password_form_submitted());
      ASSERT_TRUE(static_cast<bool>(fake_driver_.password_form_submitted()));
      form = *(fake_driver_.password_form_submitted());
    } else {
      ASSERT_EQ(PasswordFormInPageNavigation, expected_type);
      ASSERT_TRUE(fake_driver_.called_inpage_navigation());
      ASSERT_TRUE(
          static_cast<bool>(fake_driver_.password_form_inpage_navigation()));
      form = *(fake_driver_.password_form_inpage_navigation());
    }

    size_t unchecked_masks = expected_properties_masks.size();
    for (const FormFieldData& field : form.form_data.fields) {
      const auto& it = expected_properties_masks.find(field.name);
      if (it == expected_properties_masks.end())
        continue;
      EXPECT_EQ(field.properties_mask, it->second)
          << "Wrong mask for the field " << field.name;
      unchecked_masks--;
    }
    EXPECT_TRUE(unchecked_masks == 0)
        << "Some expected masks are missed in FormData";
  }

  void ExpectInPageNavigationWithUsernameAndPasswords(
      const std::string& username_value,
      const std::string& password_value,
      const std::string& new_password_value) {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(fake_driver_.called_inpage_navigation());
    ASSERT_TRUE(
        static_cast<bool>(fake_driver_.password_form_inpage_navigation()));
    const autofill::PasswordForm& form =
        *(fake_driver_.password_form_inpage_navigation());
    EXPECT_EQ(ASCIIToUTF16(username_value), form.username_value);
    EXPECT_EQ(ASCIIToUTF16(password_value), form.password_value);
    EXPECT_EQ(ASCIIToUTF16(new_password_value), form.new_password_value);
  }

  bool GetCalledShowPasswordGenerationPopup() {
    fake_pw_client_.Flush();
    return fake_pw_client_.called_show_pw_generation_popup();
  }

  void BindPasswordManagerDriver(mojo::ScopedMessagePipeHandle handle) {
    fake_driver_.BindRequest(
        mojo::MakeRequest<mojom::PasswordManagerDriver>(std::move(handle)));
  }

  void BindPasswordManagerClient(mojo::ScopedInterfaceEndpointHandle handle) {
    fake_pw_client_.BindRequest(
        mojo::MakeAssociatedRequest<mojom::PasswordManagerClient>(
            std::move(handle)));
  }

  FakeContentPasswordManagerDriver fake_driver_;
  FakePasswordManagerClient fake_pw_client_;

  base::string16 username1_;
  base::string16 username2_;
  base::string16 username3_;
  base::string16 password1_;
  base::string16 password2_;
  base::string16 password3_;
  base::string16 alternate_username3_;
  PasswordFormFillData fill_data_;

  WebInputElement username_element_;
  WebInputElement password_element_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillAgentTest);
};

// Tests that the password login is autocompleted as expected when the browser
// sends back the password info.
TEST_F(PasswordAutofillAgentTest, InitialAutocomplete) {
  /*
   * Right now we are not sending the message to the browser because we are
   * loading a data URL and the security origin canAccessPasswordManager()
   * returns false.  May be we should mock URL loading to cirmcuvent this?
   TODO(jcivelli): find a way to make the security origin not deny access to the
                   password manager and then reenable this code.

  // The form has been loaded, we should have sent the browser a message about
  // the form.
  const IPC::Message* msg = render_thread_.sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsParsed::ID);
  ASSERT_TRUE(msg != NULL);

  std::tuple<std::vector<PasswordForm> > forms;
  AutofillHostMsg_PasswordFormsParsed::Read(msg, &forms);
  ASSERT_EQ(1U, forms.a.size());
  PasswordForm password_form = forms.a[0];
  EXPECT_EQ(PasswordForm::SCHEME_HTML, password_form.scheme);
  EXPECT_EQ(ASCIIToUTF16(kUsernameName), password_form.username_element);
  EXPECT_EQ(ASCIIToUTF16(kPasswordName), password_form.password_element);
  */

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that we correctly fill forms having an empty 'action' attribute.
TEST_F(PasswordAutofillAgentTest, InitialAutocompleteForEmptyAction) {
  const char kEmptyActionFormHTML[] =
      "<FORM name='LoginTestForm'>"
      "  <INPUT type='text' id='username'/>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kEmptyActionFormHTML);

  // Retrieve the input elements so the test can access them.
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kUsernameName));
  ASSERT_FALSE(element.isNull());
  username_element_ = element.to<blink::WebInputElement>();
  element = document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();

  // Set the expected form origin and action URLs.
  UpdateOriginForHTML(kEmptyActionFormHTML);
  fill_data_.action = GURL();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that if a password is marked as readonly, neither field is autofilled
// on page load.
TEST_F(PasswordAutofillAgentTest, NoInitialAutocompleteForReadOnlyPassword) {
  SetElementReadOnly(password_element_, true);

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsState(std::string(), false, std::string(), false);
}

// Can still fill a password field if the username is set to a value that
// matches.
TEST_F(PasswordAutofillAgentTest,
       AutocompletePasswordForReadonlyUsernameMatched) {
  username_element_.setValue(WebString::fromUTF16(username3_));
  SetElementReadOnly(username_element_, true);

  // Filled even though username is not the preferred match.
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsState(UTF16ToUTF8(username3_), false,
                       UTF16ToUTF8(password3_), true);
}

// If a username field is empty and readonly, don't autofill.
TEST_F(PasswordAutofillAgentTest,
       NoAutocompletePasswordForReadonlyUsernameUnmatched) {
  username_element_.setValue(WebString::fromUTF8(""));
  SetElementReadOnly(username_element_, true);

  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

// Tests that having a non-matching username precludes the autocomplete.
TEST_F(PasswordAutofillAgentTest, NoAutocompleteForFilledFieldUnmatched) {
  username_element_.setValue(WebString::fromUTF8("bogus"));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckTextFieldsState("bogus", false, std::string(), false);
}

// Don't try to complete a prefilled value even if it's a partial match
// to a username.
TEST_F(PasswordAutofillAgentTest, NoPartialMatchForPrefilledUsername) {
  username_element_.setValue(WebString::fromUTF8("ali"));

  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsState("ali", false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, InputWithNoForms) {
  const char kNoFormInputs[] =
      "<input type='text' id='username'/>"
      "<input type='password' id='password'/>";
  LoadHTML(kNoFormInputs);

  SimulateOnFillPasswordForm(fill_data_);

  // Input elements that aren't in a <form> won't autofill.
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, NoAutocompleteForTextFieldPasswords) {
  const char kTextFieldPasswordFormHTML[] =
      "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
      "  <INPUT type='text' id='username'/>"
      "  <INPUT type='text' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kTextFieldPasswordFormHTML);

  // Retrieve the input elements so the test can access them.
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kUsernameName));
  ASSERT_FALSE(element.isNull());
  username_element_ = element.to<blink::WebInputElement>();
  element = document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();

  // Set the expected form origin URL.
  UpdateOriginForHTML(kTextFieldPasswordFormHTML);

  SimulateOnFillPasswordForm(fill_data_);

  // Fields should still be empty.
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, NoAutocompleteForPasswordFieldUsernames) {
  const char kPasswordFieldUsernameFormHTML[] =
      "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
      "  <INPUT type='password' id='username'/>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kPasswordFieldUsernameFormHTML);

  // Retrieve the input elements so the test can access them.
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kUsernameName));
  ASSERT_FALSE(element.isNull());
  username_element_ = element.to<blink::WebInputElement>();
  element = document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();

  // Set the expected form origin URL.
  UpdateOriginForHTML(kPasswordFieldUsernameFormHTML);

  SimulateOnFillPasswordForm(fill_data_);

  // Fields should still be empty.
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

// Tests that having a matching username does not preclude the autocomplete.
TEST_F(PasswordAutofillAgentTest, InitialAutocompleteForMatchingFilledField) {
  username_element_.setValue(WebString::fromUTF8(kAliceUsername));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

TEST_F(PasswordAutofillAgentTest, PasswordNotClearedOnEdit) {
  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate the user changing the username to some unknown username.
  SimulateUsernameChange("alicia");

  // The password should not have been cleared.
  CheckTextFieldsDOMState("alicia", false, kAlicePassword, true);
}

// Tests that lost focus does not trigger filling when |wait_for_username| is
// true.
TEST_F(PasswordAutofillAgentTest, WaitUsername) {
  // Simulate the browser sending back the login info.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // No auto-fill should have taken place.
  CheckTextFieldsState(std::string(), false, std::string(), false);

  SimulateUsernameChange(kAliceUsername);
  // Change focus in between to make sure blur events don't trigger filling.
  SetFocused(password_element_);
  SetFocused(username_element_);
  // No autocomplete should happen when text is entered in the username.
  CheckTextFieldsState(kAliceUsername, false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, IsWebNodeVisibleTest) {
  blink::WebVector<WebFormElement> forms1, forms2, forms3;
  blink::WebFrame* frame;

  LoadHTML(kVisibleFormWithNoUsernameHTML);
  frame = GetMainFrame();
  frame->document().forms(forms1);
  ASSERT_EQ(1u, forms1.size());
  EXPECT_TRUE(form_util::IsWebNodeVisible(forms1[0]));

  LoadHTML(kEmptyFormHTML);
  frame = GetMainFrame();
  frame->document().forms(forms2);
  ASSERT_EQ(1u, forms2.size());
  EXPECT_FALSE(form_util::IsWebNodeVisible(forms2[0]));

  LoadHTML(kNonVisibleFormHTML);
  frame = GetMainFrame();
  frame->document().forms(forms3);
  ASSERT_EQ(1u, forms3.size());
  EXPECT_FALSE(form_util::IsWebNodeVisible(forms3[0]));
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest) {
  fake_driver_.reset_password_forms_rendered();
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(static_cast<bool>(fake_driver_.password_forms_rendered()));
  EXPECT_FALSE(fake_driver_.password_forms_rendered()->empty());

  fake_driver_.reset_password_forms_rendered();
  LoadHTML(kEmptyFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(static_cast<bool>(fake_driver_.password_forms_rendered()));
  EXPECT_TRUE(fake_driver_.password_forms_rendered()->empty());

  fake_driver_.reset_password_forms_rendered();
  LoadHTML(kNonVisibleFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(static_cast<bool>(fake_driver_.password_forms_rendered()));
  EXPECT_TRUE(fake_driver_.password_forms_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_Redirection) {
  fake_driver_.reset_password_forms_rendered();
  LoadHTML(kEmptyWebpage);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_password_forms_rendered());

  fake_driver_.reset_password_forms_rendered();
  LoadHTML(kRedirectionWebpage);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_password_forms_rendered());

  fake_driver_.reset_password_forms_rendered();
  LoadHTML(kSimpleWebpage);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());

  fake_driver_.reset_password_forms_rendered();
  LoadHTML(kWebpageWithDynamicContent);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
}

// Tests that a password will only be filled as a suggested and will not be
// accessible by the DOM until a user gesture has occurred.
TEST_F(PasswordAutofillAgentTest, GestureRequiredTest) {
  // Trigger the initial autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);

  // However, it should only have completed with the suggested value, as tested
  // above, and it should not have completed into the DOM accessible value for
  // the password field.
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), true);

  // Simulate a user click so that the password field's real value is filled.
  SimulateElementClick(kUsernameName);
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// Verfies that a DOM-activated UI event will not cause an autofill.
TEST_F(PasswordAutofillAgentTest, NoDOMActivationTest) {
  // Trigger the initial autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  ExecuteJavaScriptForTests(kJavaScriptClick);
  CheckTextFieldsDOMState(kAliceUsername, true, "", true);
}

// Verifies that password autofill triggers onChange events in JavaScript for
// forms that are filled on page load.
TEST_F(PasswordAutofillAgentTest,
       PasswordAutofillTriggersOnChangeEventsOnLoad) {
  std::string html = std::string(kFormHTML) + kOnChangeDetectionScript;
  LoadHTML(html.c_str());
  UpdateOriginForHTML(html);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted...
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
  // ... but since there hasn't been a user gesture yet, the autocompleted
  // password should only be visible to the user.
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), true);

  // A JavaScript onChange event should have been triggered for the username,
  // but not yet for the password.
  int username_onchange_called = -1;
  int password_onchange_called = -1;
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("usernameOnchangeCalled ? 1 : 0"),
          &username_onchange_called));
  EXPECT_EQ(1, username_onchange_called);
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("passwordOnchangeCalled ? 1 : 0"),
          &password_onchange_called));
  // TODO(isherman): Re-enable this check once http://crbug.com/333144 is fixed.
  // EXPECT_EQ(0, password_onchange_called);

  // Simulate a user click so that the password field's real value is filled.
  SimulateElementClick(kUsernameName);
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  // Now, a JavaScript onChange event should have been triggered for the
  // password as well.
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("passwordOnchangeCalled ? 1 : 0"),
          &password_onchange_called));
  EXPECT_EQ(1, password_onchange_called);
}

// Verifies that password autofill triggers onChange events in JavaScript for
// forms that are filled after page load.
TEST_F(PasswordAutofillAgentTest,
       PasswordAutofillTriggersOnChangeEventsWaitForUsername) {
  std::string html = std::string(kFormHTML) + kOnChangeDetectionScript;
  LoadHTML(html.c_str());
  UpdateOriginForHTML(html);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should not yet have been autocompleted.
  CheckTextFieldsState(std::string(), false, std::string(), false);

  // Simulate a click just to force a user gesture, since the username value is
  // set directly.
  SimulateElementClick(kUsernameName);

  // Simulate the user entering the first letter of their username and selecting
  // the matching autofill from the dropdown.
  SimulateUsernameChange("a");
  SimulateSuggestionChoice(username_element_);

  // The username and password should now have been autocompleted.
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  // JavaScript onChange events should have been triggered both for the username
  // and for the password.
  int username_onchange_called = -1;
  int password_onchange_called = -1;
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("usernameOnchangeCalled ? 1 : 0"),
          &username_onchange_called));
  EXPECT_EQ(1, username_onchange_called);
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("passwordOnchangeCalled ? 1 : 0"),
          &password_onchange_called));
  EXPECT_EQ(1, password_onchange_called);
}

// Tests that |FillSuggestion| properly fills the username and password.
TEST_F(PasswordAutofillAgentTest, FillSuggestion) {
  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // If the password field is not autocompletable, it should not be affected.
  SetElementReadOnly(password_element_, true);
  EXPECT_FALSE(password_autofill_agent_->FillSuggestion(
      username_element_, ASCIIToUTF16(kAliceUsername),
      ASCIIToUTF16(kAlicePassword)));
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);
  SetElementReadOnly(password_element_, false);

  // After filling with the suggestion, both fields should be autocompleted.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      username_element_, ASCIIToUTF16(kAliceUsername),
      ASCIIToUTF16(kAlicePassword)));
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
  int username_length = strlen(kAliceUsername);
  CheckUsernameSelection(username_length, username_length);

  // Try Filling with a suggestion with password different from the one that was
  // initially sent to the renderer.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      username_element_, ASCIIToUTF16(kBobUsername),
      ASCIIToUTF16(kCarolPassword)));
  CheckTextFieldsDOMState(kBobUsername, true, kCarolPassword, true);
  username_length = strlen(kBobUsername);
  CheckUsernameSelection(username_length, username_length);
}

// Tests that |FillSuggestion| properly fills the password if username is
// read-only.
TEST_F(PasswordAutofillAgentTest, FillSuggestionIfUsernameReadOnly) {
  // Simulate the browser sending the login info.
  SetElementReadOnly(username_element_, true);
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // Username field is not autocompletable, it should not be affected.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      password_element_, ASCIIToUTF16(kAliceUsername),
      ASCIIToUTF16(kAlicePassword)));
  CheckTextFieldsDOMState(std::string(), false, kAlicePassword, true);

  // Try Filling with a suggestion with password different from the one that was
  // initially sent to the renderer.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      password_element_, ASCIIToUTF16(kBobUsername),
      ASCIIToUTF16(kCarolPassword)));
  CheckTextFieldsDOMState(std::string(), false, kCarolPassword, true);
}

// Tests that |PreviewSuggestion| properly previews the username and password.
TEST_F(PasswordAutofillAgentTest, PreviewSuggestion) {
  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // If the password field is not autocompletable, it should not be affected.
  SetElementReadOnly(password_element_, true);
  EXPECT_FALSE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));
  EXPECT_EQ(std::string(), username_element_.suggestedValue().utf8());
  EXPECT_FALSE(username_element_.isAutofilled());
  EXPECT_EQ(std::string(), password_element_.suggestedValue().utf8());
  EXPECT_FALSE(password_element_.isAutofilled());
  SetElementReadOnly(password_element_, false);

  // After selecting the suggestion, both fields should be previewed
  // with suggested values.
  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));
  EXPECT_EQ(kAliceUsername, username_element_.suggestedValue().utf8());
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_EQ(kAlicePassword, password_element_.suggestedValue().utf8());
  EXPECT_TRUE(password_element_.isAutofilled());
  int username_length = strlen(kAliceUsername);
  CheckUsernameSelection(0, username_length);

  // Try previewing with a password different from the one that was initially
  // sent to the renderer.
  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kBobUsername, kCarolPassword));
  EXPECT_EQ(kBobUsername, username_element_.suggestedValue().utf8());
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_EQ(kCarolPassword, password_element_.suggestedValue().utf8());
  EXPECT_TRUE(password_element_.isAutofilled());
  username_length = strlen(kBobUsername);
  CheckUsernameSelection(0, username_length);
}

// Tests that |PreviewSuggestion| properly previews the password if username is
// read-only.
TEST_F(PasswordAutofillAgentTest, PreviewSuggestionIfUsernameReadOnly) {
  // Simulate the browser sending the login info.
  SetElementReadOnly(username_element_, true);
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // Username field is not autocompletable, it should not be affected.
  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      password_element_, kAliceUsername, kAlicePassword));
  EXPECT_EQ(std::string(), username_element_.suggestedValue().utf8());
  EXPECT_FALSE(username_element_.isAutofilled());

  // Password field must be autofilled.
  EXPECT_EQ(kAlicePassword, password_element_.suggestedValue().utf8());
  EXPECT_TRUE(password_element_.isAutofilled());

  // Try previewing with a password different from the one that was initially
  // sent to the renderer.
  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      password_element_, kBobUsername, kCarolPassword));
  EXPECT_EQ(std::string(), username_element_.suggestedValue().utf8());
  EXPECT_FALSE(username_element_.isAutofilled());
  EXPECT_EQ(kCarolPassword, password_element_.suggestedValue().utf8());
  EXPECT_TRUE(password_element_.isAutofilled());
}

// Tests that |PreviewSuggestion| properly sets the username selection range.
TEST_F(PasswordAutofillAgentTest, PreviewSuggestionSelectionRange) {
  username_element_.setValue(WebString::fromUTF8("ali"));
  username_element_.setSelectionRange(3, 3);
  username_element_.setAutofilled(true);

  CheckTextFieldsDOMState("ali", true, std::string(), false);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));
  EXPECT_EQ(kAliceUsername, username_element_.suggestedValue().utf8());
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_EQ(kAlicePassword, password_element_.suggestedValue().utf8());
  EXPECT_TRUE(password_element_.isAutofilled());
  int username_length = strlen(kAliceUsername);
  CheckUsernameSelection(3, username_length);
}

// Tests that |ClearPreview| properly clears previewed username and password
// with password being previously autofilled.
TEST_F(PasswordAutofillAgentTest, ClearPreviewWithPasswordAutofilled) {
  password_element_.setValue(WebString::fromUTF8("sec"));
  password_element_.setAutofilled(true);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState(std::string(), false, "sec", true);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));

  EXPECT_TRUE(
      password_autofill_agent_->DidClearAutofillSelection(username_element_));

  EXPECT_TRUE(username_element_.value().isEmpty());
  EXPECT_TRUE(username_element_.suggestedValue().isEmpty());
  EXPECT_FALSE(username_element_.isAutofilled());
  EXPECT_EQ(ASCIIToUTF16("sec"), password_element_.value().utf16());
  EXPECT_TRUE(password_element_.suggestedValue().isEmpty());
  EXPECT_TRUE(password_element_.isAutofilled());
  CheckUsernameSelection(0, 0);
}

// Tests that |ClearPreview| properly clears previewed username and password
// with username being previously autofilled.
TEST_F(PasswordAutofillAgentTest, ClearPreviewWithUsernameAutofilled) {
  username_element_.setValue(WebString::fromUTF8("ali"));
  username_element_.setSelectionRange(3, 3);
  username_element_.setAutofilled(true);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("ali", true, std::string(), false);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));

  EXPECT_TRUE(
      password_autofill_agent_->DidClearAutofillSelection(username_element_));

  EXPECT_EQ(ASCIIToUTF16("ali"), username_element_.value().utf16());
  EXPECT_TRUE(username_element_.suggestedValue().isEmpty());
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_TRUE(password_element_.value().isEmpty());
  EXPECT_TRUE(password_element_.suggestedValue().isEmpty());
  EXPECT_FALSE(password_element_.isAutofilled());
  CheckUsernameSelection(3, 3);
}

// Tests that |ClearPreview| properly clears previewed username and password
// with username and password being previously autofilled.
TEST_F(PasswordAutofillAgentTest,
       ClearPreviewWithAutofilledUsernameAndPassword) {
  username_element_.setValue(WebString::fromUTF8("ali"));
  username_element_.setSelectionRange(3, 3);
  username_element_.setAutofilled(true);
  password_element_.setValue(WebString::fromUTF8("sec"));
  password_element_.setAutofilled(true);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("ali", true, "sec", true);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));

  EXPECT_TRUE(
      password_autofill_agent_->DidClearAutofillSelection(username_element_));

  EXPECT_EQ(ASCIIToUTF16("ali"), username_element_.value().utf16());
  EXPECT_TRUE(username_element_.suggestedValue().isEmpty());
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_EQ(ASCIIToUTF16("sec"), password_element_.value().utf16());
  EXPECT_TRUE(password_element_.suggestedValue().isEmpty());
  EXPECT_TRUE(password_element_.isAutofilled());
  CheckUsernameSelection(3, 3);
}

// Tests that |ClearPreview| properly clears previewed username and password
// with neither username nor password being previously autofilled.
TEST_F(PasswordAutofillAgentTest,
       ClearPreviewWithNotAutofilledUsernameAndPassword) {
  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));

  EXPECT_TRUE(
      password_autofill_agent_->DidClearAutofillSelection(username_element_));

  EXPECT_TRUE(username_element_.value().isEmpty());
  EXPECT_TRUE(username_element_.suggestedValue().isEmpty());
  EXPECT_FALSE(username_element_.isAutofilled());
  EXPECT_TRUE(password_element_.value().isEmpty());
  EXPECT_TRUE(password_element_.suggestedValue().isEmpty());
  EXPECT_FALSE(password_element_.isAutofilled());
  CheckUsernameSelection(0, 0);
}

// Tests that logging is off by default.
TEST_F(PasswordAutofillAgentTest, OnChangeLoggingState_NoMessage) {
  SendVisiblePasswordForms();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_record_save_progress());
}

// Test that logging can be turned on by a message.
TEST_F(PasswordAutofillAgentTest, OnChangeLoggingState_Activated) {
  // Turn the logging on.
  password_autofill_agent_->SetLoggingState(true);

  SendVisiblePasswordForms();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_record_save_progress());
}

// Test that logging can be turned off by a message.
TEST_F(PasswordAutofillAgentTest, OnChangeLoggingState_Deactivated) {
  // Turn the logging on and then off.
  password_autofill_agent_->SetLoggingState(true);
  password_autofill_agent_->SetLoggingState(false);

  SendVisiblePasswordForms();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_record_save_progress());
}

// Tests that one user click on a username field is sufficient to bring up a
// credential suggestion popup, and the user can autocomplete the password by
// selecting the credential from the popup.
TEST_F(PasswordAutofillAgentTest, ClickAndSelect) {
  // SimulateElementClick() is called so that a user gesture is actually made
  // and the password can be filled. However, SimulateElementClick() does not
  // actually lead to the AutofillAgent's InputElementClicked() method being
  // called, so SimulateSuggestionChoice has to manually call
  // InputElementClicked().
  ClearUsernameAndPasswordFields();
  SimulateOnFillPasswordForm(fill_data_);
  SimulateElementClick(kUsernameName);
  SimulateSuggestionChoice(username_element_);
  CheckSuggestions(kAliceUsername, true);

  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// Tests the autosuggestions that are given when the element is clicked.
// Specifically, tests when the user clicks on the username element after page
// load and the element is autofilled, when the user clicks on an element that
// has a non-matching username, and when the user clicks on an element that's
// already been autofilled and they've already modified.
TEST_F(PasswordAutofillAgentTest, CredentialsOnClick) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);

  // Simulate a user clicking on the username element. This should produce a
  // message with all the usernames.
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(username_element_, false);
  CheckSuggestions(std::string(), false);

  // Now simulate a user typing in an unrecognized username and then
  // clicking on the username element. This should also produce a message with
  // all the usernames.
  SimulateUsernameChange("baz");
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(username_element_, true);
  CheckSuggestions("baz", true);
  ClearUsernameAndPasswordFields();
}

// Tests that there is an autosuggestion from the password manager when the
// user clicks on the password field when FillOnAccountSelect is enabled.
TEST_F(PasswordAutofillAgentTest,
       FillOnAccountSelectOnlyNoCredentialsOnPasswordClick) {
  SetFillOnAccountSelect();

  // Simulate the browser sending back the login info.
  SimulateOnShowInitialPasswordAccountSuggestions(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);

  // Simulate a user clicking on the password element. This should produce no
  // message.
  fake_driver_.reset_show_pw_suggestions();
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(password_element_, false);
  EXPECT_TRUE(GetCalledShowPasswordSuggestions());
}

// Tests the autosuggestions that are given when a password element is clicked,
// the username element is not editable, and FillOnAccountSelect is enabled.
// Specifically, tests when the user clicks on the password element after page
// load, and the corresponding username element is readonly (and thus
// uneditable), that the credentials for the already-filled username are
// suggested.
TEST_F(PasswordAutofillAgentTest,
       FillOnAccountSelectOnlyCredentialsOnPasswordClick) {
  SetFillOnAccountSelect();

  // Simulate the browser sending back the login info.
  SimulateOnShowInitialPasswordAccountSuggestions(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // Simulate the page loading with a prefilled username element that is
  // uneditable.
  username_element_.setValue("alicia");
  SetElementReadOnly(username_element_, true);

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);

  // Simulate a user clicking on the password element. This should produce a
  // dropdown with suggestion of all available usernames and so username
  // filter will be the empty string.
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(password_element_, false);
  CheckSuggestions("", false);
}

// Tests that there is an autosuggestion from the password manager when the
// user clicks on the password field.
TEST_F(PasswordAutofillAgentTest, NoCredentialsOnPasswordClick) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);

  // Simulate a user clicking on the password element. This should produce no
  // message.
  fake_driver_.reset_show_pw_suggestions();
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(password_element_, false);
  EXPECT_TRUE(GetCalledShowPasswordSuggestions());
}

// The user types in a username and a password, but then just before sending
// the form off, a script clears them. This test checks that
// PasswordAutofillAgent can still remember the username and the password
// typed by the user.
TEST_F(PasswordAutofillAgentTest,
       RememberLastNonEmptyUsernameAndPasswordOnSubmit_ScriptCleared) {
  SimulateUsernameChange("temp");
  SimulatePasswordChange("random");

  // Simulate that the username and the password value was cleared by the
  // site's JavaScript before submit.
  username_element_.setValue(WebString());
  password_element_.setValue(WebString());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the last non-empty
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "random", "");
}

// Similar to RememberLastNonEmptyPasswordOnSubmit_ScriptCleared, but this time
// it's the user who clears the username and the password. This test checks
// that in that case, the last non-empty username and password are not
// remembered.
TEST_F(PasswordAutofillAgentTest,
       RememberLastNonEmptyUsernameAndPasswordOnSubmit_UserCleared) {
  SimulateUsernameChange("temp");
  SimulatePasswordChange("random");

  // Simulate that the user actually cleared the username and password again.
  SimulateUsernameChange("");
  SimulatePasswordChange("");
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent respects the user having cleared the
  // password.
  ExpectFormSubmittedWithUsernameAndPasswords("", "", "");
}

// Similar to RememberLastNonEmptyPasswordOnSubmit_ScriptCleared, but uses the
// new password instead of the current password.
TEST_F(PasswordAutofillAgentTest,
       RememberLastNonEmptyUsernameAndPasswordOnSubmit_New) {
  const char kNewPasswordFormHTML[] =
      "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
      "  <INPUT type='text' id='username' autocomplete='username'/>"
      "  <INPUT type='password' id='password' autocomplete='new-password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kNewPasswordFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameChange("temp");
  SimulatePasswordChange("random");

  // Simulate that the username and the password value was cleared by
  // the site's JavaScript before submit.
  username_element_.setValue(WebString());
  password_element_.setValue(WebString());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the last non-empty
  // password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "", "random");
}

// The user first accepts a suggestion, but then overwrites the password. This
// test checks that the overwritten password is not reverted back.
TEST_F(PasswordAutofillAgentTest,
       NoopEditingDoesNotOverwriteManuallyEditedPassword) {
  fill_data_.wait_for_username = true;
  SimulateUsernameChange(kAliceUsername);
  SimulateOnFillPasswordForm(fill_data_);
  SimulateSuggestionChoice(username_element_);
  const std::string old_username(username_element_.value().utf8());
  const std::string old_password(password_element_.value().utf8());
  const std::string new_password(old_password + "modify");

  // The user changes the password.
  SimulatePasswordChange(new_password);

  // Change focus in between to make sure blur events don't trigger filling.
  SetFocused(password_element_);
  SetFocused(username_element_);

  // The password should have stayed as the user changed it.
  CheckTextFieldsDOMState(old_username, true, new_password, false);
  // The password should not have a suggested value.
  CheckTextFieldsState(old_username, true, std::string(), false);
}

// The user types in a username and a password, but then just before sending
// the form off, a script changes them. This test checks that
// PasswordAutofillAgent can still remember the username and the password
// typed by the user.
TEST_F(PasswordAutofillAgentTest,
       RememberLastTypedUsernameAndPasswordOnSubmit_ScriptChanged) {
  SimulateUsernameChange("temp");
  SimulatePasswordChange("random");

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.setValue(WebString("new username"));
  password_element_.setValue(WebString("new password"));
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "random", "");
}

TEST_F(PasswordAutofillAgentTest, RememberFieldPropertiesOnSubmit) {
  SimulateUsernameChange("temp");
  SimulatePasswordChange("random");

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.setValue(WebString("new username"));
  password_element_.setValue(WebString("new password"));
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  std::map<base::string16, FieldPropertiesMask> expected_properties_masks;
  expected_properties_masks[ASCIIToUTF16("random_field")] =
      FieldPropertiesFlags::NO_FLAGS;
  expected_properties_masks[ASCIIToUTF16("username")] =
      FieldPropertiesFlags::USER_TYPED;
  expected_properties_masks[ASCIIToUTF16("password")] =
      FieldPropertiesFlags::USER_TYPED | FieldPropertiesFlags::HAD_FOCUS;

  ExpectFieldPropertiesMasks(PasswordFormSubmitted, expected_properties_masks);
}

TEST_F(PasswordAutofillAgentTest, RememberFieldPropertiesOnInPageNavigation) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameChange("Bob");
  SimulatePasswordChange("mypassword");

  username_element_.setAttribute("style", "display:none;");
  password_element_.setAttribute("style", "display:none;");

  password_autofill_agent_->AJAXSucceeded();

  std::map<base::string16, FieldPropertiesMask> expected_properties_masks;
  expected_properties_masks[ASCIIToUTF16("username")] =
      FieldPropertiesFlags::USER_TYPED;
  expected_properties_masks[ASCIIToUTF16("password")] =
      FieldPropertiesFlags::USER_TYPED | FieldPropertiesFlags::HAD_FOCUS;

  ExpectFieldPropertiesMasks(PasswordFormInPageNavigation,
                             expected_properties_masks);
}

// The username/password is autofilled by password manager then just before
// sending the form off, a script changes them. This test checks that
// PasswordAutofillAgent can still get the username and the password autofilled.
TEST_F(PasswordAutofillAgentTest,
       RememberLastAutofilledUsernameAndPasswordOnSubmit_ScriptChanged) {
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.setValue(WebString("new username"));
  password_element_.setValue(WebString("new password"));
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the autofilled
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(kAliceUsername, kAlicePassword,
                                              "");
}

// The username/password is autofilled by password manager then user types in a
// username and a password. Then just before sending the form off, a script
// changes them. This test checks that PasswordAutofillAgent can still remember
// the username and the password typed by the user.
TEST_F(
    PasswordAutofillAgentTest,
    RememberLastTypedAfterAutofilledUsernameAndPasswordOnSubmit_ScriptChanged) {
  SimulateOnFillPasswordForm(fill_data_);

  SimulateUsernameChange("temp");
  SimulatePasswordChange("random");

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.setValue(WebString("new username"));
  password_element_.setValue(WebString("new password"));
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "random", "");
}

// The user starts typing username then it is autofilled.
// PasswordAutofillAgent should remember the username that was autofilled,
// not last typed.
TEST_F(PasswordAutofillAgentTest, RememberAutofilledUsername) {
  SimulateUsernameChange("Te");
  // Simulate that the username was changed by autofilling.
  username_element_.setValue(WebString("temp"));
  SimulatePasswordChange("random");

  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "random", "");
}

TEST_F(PasswordAutofillAgentTest, FormFillDataMustHaveUsername) {
  ClearUsernameAndPasswordFields();

  PasswordFormFillData no_username_fill_data = fill_data_;
  no_username_fill_data.username_field.name = base::string16();
  SimulateOnFillPasswordForm(no_username_fill_data);

  // The username and password should not have been autocompleted.
  CheckTextFieldsState("", false, "", false);
}

TEST_F(PasswordAutofillAgentTest, FillOnAccountSelectOnly) {
  SetFillOnAccountSelect();

  ClearUsernameAndPasswordFields();

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnShowInitialPasswordAccountSuggestions(fill_data_);

  CheckTextFieldsState(std::string(), false, std::string(), false);
  CheckSuggestions(std::string(), true);
}

TEST_F(PasswordAutofillAgentTest, FillOnAccountSelectOnlyReadonlyUsername) {
  SetFillOnAccountSelect();

  ClearUsernameAndPasswordFields();

  username_element_.setValue("alice");
  SetElementReadOnly(username_element_, true);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnShowInitialPasswordAccountSuggestions(fill_data_);

  CheckTextFieldsState(std::string("alice"), false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest,
       FillOnAccountSelectOnlyReadonlyNotPreferredUsername) {
  SetFillOnAccountSelect();

  ClearUsernameAndPasswordFields();

  username_element_.setValue("Carol");
  SetElementReadOnly(username_element_, true);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnShowInitialPasswordAccountSuggestions(fill_data_);

  CheckTextFieldsState(std::string("Carol"), false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, FillOnAccountSelectOnlyNoUsername) {
  SetFillOnAccountSelect();

  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  username_element_.reset();
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();
  fill_data_.username_field = FormFieldData();
  UpdateOriginForHTML(kVisibleFormWithNoUsernameHTML);
  fill_data_.additional_logins.clear();
  fill_data_.other_possible_usernames.clear();

  password_element_.setValue("");
  password_element_.setAutofilled(false);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnShowInitialPasswordAccountSuggestions(fill_data_);

  EXPECT_TRUE(password_element_.suggestedValue().isEmpty());
  EXPECT_FALSE(password_element_.isAutofilled());
  CheckSuggestions(std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, ShowPopupOnEmptyPasswordField) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  username_element_.reset();
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();
  fill_data_.username_field = FormFieldData();
  UpdateOriginForHTML(kVisibleFormWithNoUsernameHTML);
  fill_data_.additional_logins.clear();
  fill_data_.other_possible_usernames.clear();

  password_element_.setValue("");
  password_element_.setAutofilled(false);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  // Show popup suggesstion when the password field is empty.
  password_element_.setValue("");
  password_element_.setAutofilled(false);

  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, base::string16(), ASCIIToUTF16(kAlicePassword));
  CheckSuggestions(std::string(), false);
  EXPECT_EQ(ASCIIToUTF16(kAlicePassword), password_element_.value().utf16());
  EXPECT_TRUE(password_element_.isAutofilled());
}

TEST_F(PasswordAutofillAgentTest, ShowPopupOnAutofilledPasswordField) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  username_element_.reset();
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();
  fill_data_.username_field = FormFieldData();
  UpdateOriginForHTML(kVisibleFormWithNoUsernameHTML);
  fill_data_.additional_logins.clear();
  fill_data_.other_possible_usernames.clear();

  password_element_.setValue("");
  password_element_.setAutofilled(false);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  // Show popup suggesstion when the password field is autofilled.
  password_element_.setValue("123");
  password_element_.setAutofilled(true);

  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, base::string16(), ASCIIToUTF16(kAlicePassword));
  CheckSuggestions(std::string(), false);
  EXPECT_EQ(ASCIIToUTF16(kAlicePassword), password_element_.value().utf16());
  EXPECT_TRUE(password_element_.isAutofilled());
}

TEST_F(PasswordAutofillAgentTest, NotShowPopupPasswordField) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  username_element_.reset();
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();
  fill_data_.username_field = FormFieldData();
  UpdateOriginForHTML(kVisibleFormWithNoUsernameHTML);
  fill_data_.additional_logins.clear();
  fill_data_.other_possible_usernames.clear();

  password_element_.setValue("");
  password_element_.setAutofilled(false);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  // Do not show popup suggesstion when the password field is not-empty and not
  // autofilled.
  password_element_.setValue("123");
  password_element_.setAutofilled(false);

  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, base::string16(), ASCIIToUTF16(kAlicePassword));
  ASSERT_FALSE(GetCalledShowPasswordSuggestions());
}

// Tests with fill-on-account-select enabled that if the username element is
// read-only and filled with an unknown username, then the password field is not
// highlighted as autofillable (regression test for https://crbug.com/442564).
TEST_F(PasswordAutofillAgentTest,
       FillOnAccountSelectOnlyReadonlyUnknownUsername) {
  SetFillOnAccountSelect();

  ClearUsernameAndPasswordFields();

  username_element_.setValue("foobar");
  SetElementReadOnly(username_element_, true);

  CheckTextFieldsState(std::string("foobar"), false, std::string(), false);
}

// Test that the last plain text field before a password field is chosen as a
// username, in a form with 2 plain text fields without username predictions.
TEST_F(PasswordAutofillAgentTest, FindingUsernameWithoutAutofillPredictions) {
  LoadHTML(kFormHTMLWithTwoTextFields);
  UpdateUsernameAndPasswordElements();
  blink::WebInputElement email_element = GetInputElementByID(kEmailName);
  SimulateUsernameChange("temp");
  SimulateUserInputChangeForElement(&email_element, "temp@google.com");
  SimulatePasswordChange("random");
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent identifies the second field (e-mail)
  // as username.
  ExpectFormSubmittedWithUsernameAndPasswords("temp@google.com", "random", "");
}

// Tests that field predictions are followed when identifying the username
// and password in a password form with two plain text fields.
TEST_F(PasswordAutofillAgentTest, FindingFieldsWithAutofillPredictions) {
  LoadHTML(kFormHTMLWithTwoTextFields);
  UpdateUsernameAndPasswordElements();
  blink::WebInputElement email_element = GetInputElementByID(kEmailName);
  SimulateUsernameChange("temp");
  SimulateUserInputChangeForElement(&email_element, "temp@google.com");
  SimulatePasswordChange("random");
  // Find FormData for visible password form.
  WebFormElement form_element = username_element_.form();
  FormData form_data;
  ASSERT_TRUE(WebFormElementToFormData(
      form_element, blink::WebFormControlElement(), nullptr,
      form_util::EXTRACT_NONE, &form_data, nullptr));
  // Simulate Autofill predictions: the first field is username, the third
  // one is password.
  std::map<autofill::FormData, PasswordFormFieldPredictionMap> predictions;
  predictions[form_data][form_data.fields[0]] = PREDICTION_USERNAME;
  predictions[form_data][form_data.fields[2]] = PREDICTION_NEW_PASSWORD;
  password_autofill_agent_->AutofillUsernameAndPasswordDataReceived(
      predictions);

  // The predictions should still match even if the form changes, as long
  // as the particular elements don't change.
  std::string add_field_to_form =
      "var form = document.getElementById('LoginTestForm');"
      "var new_input = document.createElement('input');"
      "new_input.setAttribute('type', 'text');"
      "new_input.setAttribute('id', 'other_field');"
      "form.appendChild(new_input);";
  ExecuteJavaScriptForTests(add_field_to_form.c_str());

  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent identifies the first field as
  // username.
  // TODO(msramek): We should also test that adding another password field
  // won't override the password field prediction either. However, the password
  // field predictions are not taken into account yet.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "random", "");
}

// The user types in a username and a password. Then JavaScript changes password
// field to readonly state before submit. PasswordAutofillAgent can correctly
// process readonly password field. This test models behaviour of gmail.com.
TEST_F(PasswordAutofillAgentTest, ReadonlyPasswordFieldOnSubmit) {
  SimulateUsernameChange("temp");
  SimulatePasswordChange("random");

  // Simulate that JavaScript makes password field readonly.
  SetElementReadOnly(password_element_, true);
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent can correctly process submitted
  // form.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "random", "");
}

// Verify that typed passwords are saved correctly when autofill and generation
// both trigger. Regression test for https://crbug.com/493455
TEST_F(PasswordAutofillAgentTest, PasswordGenerationTriggered_TypedPassword) {
  SimulateOnFillPasswordForm(fill_data_);

  SetNotBlacklistedMessage(password_generation_, kFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 2);

  SimulateUsernameChange("NewGuy");
  SimulatePasswordChange("NewPassword");
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  ExpectFormSubmittedWithUsernameAndPasswords("NewGuy", "NewPassword", "");
}

// Verify that generated passwords are saved correctly when autofill and
// generation both trigger. Regression test for https://crbug.com/493455.
TEST_F(PasswordAutofillAgentTest,
       PasswordGenerationTriggered_GeneratedPassword) {
  SimulateOnFillPasswordForm(fill_data_);

  SetNotBlacklistedMessage(password_generation_, kFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 2);

  base::string16 password = base::ASCIIToUTF16("NewPass22");
  password_generation_->GeneratedPasswordAccepted(password);

  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  ExpectFormSubmittedWithUsernameAndPasswords(kAliceUsername, "NewPass22", "");
}

// If password generation is enabled for a field, password autofill should not
// show UI.
TEST_F(PasswordAutofillAgentTest, PasswordGenerationSupersedesAutofill) {
  LoadHTML(kSignupFormHTML);

  // Update password_element_;
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8("new_password"));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();

  // Update fill_data_ for the new form and simulate filling. Pretend as if
  // the password manager didn't detect a username field so it will try to
  // show UI when the password field is focused.
  fill_data_.wait_for_username = true;
  fill_data_.username_field = FormFieldData();
  fill_data_.password_field.name = base::ASCIIToUTF16("new_password");
  UpdateOriginForHTML(kSignupFormHTML);
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate generation triggering.
  SetNotBlacklistedMessage(password_generation_,
                           kSignupFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);

  // Simulate the field being clicked to start typing. This should trigger
  // generation but not password autofill.
  SetFocused(password_element_);
  SimulateElementClick("new_password");
  EXPECT_FALSE(GetCalledShowPasswordSuggestions());
  EXPECT_TRUE(GetCalledShowPasswordGenerationPopup());
}

// Tests that a password change form is properly filled with the username and
// password.
TEST_F(PasswordAutofillAgentTest, FillSuggestionPasswordChangeForms) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateOriginForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();
  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  fill_data_.is_possible_change_password_form = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      username_element_, ASCIIToUTF16(kAliceUsername),
      ASCIIToUTF16(kAlicePassword)));
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that a password change form is properly filled with the password when
// the user click on the password field.
TEST_F(PasswordAutofillAgentTest,
       FillSuggestionPasswordChangeFormsOnlyPassword) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateOriginForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();
  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  fill_data_.is_possible_change_password_form = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      password_element_, ASCIIToUTF16(kAliceUsername),
      ASCIIToUTF16(kAlicePassword)));
  CheckTextFieldsDOMState("", false, kAlicePassword, true);
}

// Tests that one user click on a username field is sufficient to bring up a
// credential suggestion popup on a change password form.
TEST_F(PasswordAutofillAgentTest,
       SuggestionsOnUsernameFieldOfChangePasswordForm) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateOriginForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();

  ClearUsernameAndPasswordFields();
  fill_data_.wait_for_username = true;
  fill_data_.is_possible_change_password_form = true;
  SimulateOnFillPasswordForm(fill_data_);
  // Simulate a user clicking on the username element. This should produce a
  // message.
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(username_element_, true);
  CheckSuggestions("", true);
}

// Tests that one user click on a password field is sufficient to bring up a
// credential suggestion popup on a change password form.
TEST_F(PasswordAutofillAgentTest,
       SuggestionsOnPasswordFieldOfChangePasswordForm) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateOriginForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();

  ClearUsernameAndPasswordFields();
  fill_data_.wait_for_username = true;
  fill_data_.is_possible_change_password_form = true;
  SimulateOnFillPasswordForm(fill_data_);
  // Simulate a user clicking on the password element. This should produce a
  // message.
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(password_element_, true);
  CheckSuggestions("", false);
}

// Tests that NOT_PASSWORD field predictions are followed so that no password
// form is submitted.
TEST_F(PasswordAutofillAgentTest, IgnoreNotPasswordFields) {
  LoadHTML(kCreditCardFormHTML);
  blink::WebInputElement credit_card_owner_element =
      GetInputElementByID(kCreditCardOwnerName);
  blink::WebInputElement credit_card_number_element =
      GetInputElementByID(kCreditCardNumberName);
  blink::WebInputElement credit_card_verification_element =
      GetInputElementByID(kCreditCardVerificationName);
  SimulateUserInputChangeForElement(&credit_card_owner_element, "JohnSmith");
  SimulateUserInputChangeForElement(&credit_card_number_element,
                                    "1234123412341234");
  SimulateUserInputChangeForElement(&credit_card_verification_element, "123");
  // Find FormData for visible form.
  WebFormElement form_element = credit_card_number_element.form();
  FormData form_data;
  ASSERT_TRUE(WebFormElementToFormData(
      form_element, blink::WebFormControlElement(), nullptr,
      form_util::EXTRACT_NONE, &form_data, nullptr));
  // Simulate Autofill predictions: the third field is not a password.
  std::map<autofill::FormData, PasswordFormFieldPredictionMap> predictions;
  predictions[form_data][form_data.fields[2]] = PREDICTION_NOT_PASSWORD;
  password_autofill_agent_->AutofillUsernameAndPasswordDataReceived(
      predictions);

  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(form_element);
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(form_element);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_driver_.called_password_form_submitted());
}

// Tests that only the password field is autocompleted when the browser sends
// back data with only one credentials and empty username.
TEST_F(PasswordAutofillAgentTest, NotAutofillNoUsername) {
  fill_data_.username_field.value.clear();
  fill_data_.additional_logins.clear();
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsState("", false, kAlicePassword, true);
}

// Tests that both the username and the password fields are autocompleted
// despite the empty username, when the browser sends back data more than one
// credential.
TEST_F(PasswordAutofillAgentTest,
       AutofillNoUsernameWhenOtherCredentialsStored) {
  fill_data_.username_field.value.clear();
  ASSERT_FALSE(fill_data_.additional_logins.empty());
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsState("", true, kAlicePassword, true);
}

TEST_F(PasswordAutofillAgentTest, NoForm_PromptForAJAXSubmitWithoutNavigation) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameChange("Bob");
  SimulatePasswordChange("mypassword");

  username_element_.setAttribute("style", "display:none;");
  password_element_.setAttribute("style", "display:none;");

  password_autofill_agent_->AJAXSucceeded();

  ExpectInPageNavigationWithUsernameAndPasswords("Bob", "mypassword", "");
}

TEST_F(PasswordAutofillAgentTest,
       NoForm_NoPromptForAJAXSubmitWithoutNavigationAndElementsVisible) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameChange("Bob");
  SimulatePasswordChange("mypassword");

  password_autofill_agent_->AJAXSucceeded();

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_driver_.called_password_form_submitted());
}

// Tests that credential suggestions are autofilled on a password (and change
// password) forms having either ambiguous or empty name.
TEST_F(PasswordAutofillAgentTest,
       SuggestionsOnFormContainingAmbiguousOrEmptyNames) {
  const char kEmpty[] = "";
  const char kDummyUsernameField[] = "anonymous_username";
  const char kDummyPasswordField[] = "anonymous_password";
  const char kFormContainsEmptyNamesHTML[] =
      "<FORM name='WithoutNameIdForm' action='http://www.bidule.com' >"
      "  <INPUT type='text' placeholder='username'/>"
      "  <INPUT type='password' placeholder='Password'/>"
      "  <INPUT type='submit' />"
      "</FORM>";

  const char kFormContainsAmbiguousNamesHTML[] =
      "<FORM name='AmbiguousNameIdForm' action='http://www.bidule.com' >"
      "  <INPUT type='text' id='credentials' placeholder='username' />"
      "  <INPUT type='password' id='credentials' placeholder='Password' />"
      "  <INPUT type='submit' />"
      "</FORM>";

  const char kChangePasswordFormContainsEmptyNamesHTML[] =
      "<FORM name='ChangePwd' action='http://www.bidule.com' >"
      "  <INPUT type='text' placeholder='username' />"
      "  <INPUT type='password' placeholder='Old Password' "
      "    autocomplete='current-password' />"
      "  <INPUT type='password' placeholder='New Password' "
      "    autocomplete='new-password' />"
      "  <INPUT type='submit' />"
      "</FORM>";

  const char kChangePasswordFormButNoUsername[] =
      "<FORM name='ChangePwdButNoUsername' action='http://www.bidule.com' >"
      "  <INPUT type='password' placeholder='Old Password' "
      "    autocomplete='current-password' />"
      "  <INPUT type='password' placeholder='New Password' "
      "    autocomplete='new-password' />"
      "  <INPUT type='submit' />"
      "</FORM>";

  const char kChangePasswordFormButNoOldPassword[] =
      "<FORM name='ChangePwdButNoOldPwd' action='http://www.bidule.com' >"
      "  <INPUT type='text' placeholder='username' />"
      "  <INPUT type='password' placeholder='New Password' "
      "    autocomplete='new-password' />"
      "  <INPUT type='password' placeholder='Retype Password' "
      "    autocomplete='new-password' />"
      "  <INPUT type='submit' />"
      "</FORM>";

  const char kChangePasswordFormButNoAutocompleteAttribute[] =
      "<FORM name='ChangePwdButNoAutocomplete' action='http://www.bidule.com'>"
      "  <INPUT type='text' placeholder='username' />"
      "  <INPUT type='password' placeholder='Old Password' />"
      "  <INPUT type='password' placeholder='New Password' />"
      "  <INPUT type='submit' />"
      "</FORM>";

  const struct {
    const char* html_form;
    bool is_possible_change_password_form;
    bool does_trigger_autocomplete_on_fill;
    const char* fill_data_username_field_name;
    const char* fill_data_password_field_name;
    const char* expected_username_suggestions;
    const char* expected_password_suggestions;
    bool expected_is_username_autofillable;
    bool expected_is_password_autofillable;
  } test_cases[] = {
      // Password form without name or id attributes specified for the input
      // fields.
      {kFormContainsEmptyNamesHTML, false, true, kDummyUsernameField,
       kDummyPasswordField, kAliceUsername, kAlicePassword, true, true},

      // Password form with ambiguous name or id attributes specified for the
      // input fields.
      {kFormContainsAmbiguousNamesHTML, false, true, "credentials",
       "credentials", kAliceUsername, kAlicePassword, true, true},

      // Change password form without name or id attributes specified for the
      // input fields and |autocomplete='current-password'| attribute for old
      // password field.
      {kChangePasswordFormContainsEmptyNamesHTML, true, true,
       kDummyUsernameField, kDummyPasswordField, kAliceUsername, kAlicePassword,
       true, true},

      // Change password form without username field.
      {kChangePasswordFormButNoUsername, true, true, kEmpty,
       kDummyPasswordField, kEmpty, kAlicePassword, false, true},

      // Change password form without name or id attributes specified for the
      // input fields and |autocomplete='new-password'| attribute for new
      // password fields. This form *do not* trigger |OnFillPasswordForm| from
      // browser.
      {kChangePasswordFormButNoOldPassword, true, false, kDummyUsernameField,
       kDummyPasswordField, kEmpty, kEmpty, false, false},

      // Change password form without name or id attributes specified for the
      // input fields but |autocomplete='current-password'| or
      // |autocomplete='new-password'| attributes are missing for old and new
      // password fields respectively.
      {kChangePasswordFormButNoAutocompleteAttribute, true, true,
       kDummyUsernameField, kDummyPasswordField, kAliceUsername, kAlicePassword,
       true, true},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << "html_form: " << test_case.html_form
                                    << ", fill_data_username_field_name: "
                                    << test_case.fill_data_username_field_name
                                    << ", fill_data_password_field_name: "
                                    << test_case.fill_data_password_field_name);

    // Load a password form.
    LoadHTML(test_case.html_form);

    // Get the username and password form input elelments.
    blink::WebDocument document = GetMainFrame()->document();
    blink::WebVector<WebFormElement> forms;
    document.forms(forms);
    WebFormElement form_element = forms[0];
    std::vector<blink::WebFormControlElement> control_elements =
        form_util::ExtractAutofillableElementsInForm(form_element);
    bool has_fillable_username =
        (kEmpty != test_case.fill_data_username_field_name);
    if (has_fillable_username) {
      username_element_ = control_elements[0].to<blink::WebInputElement>();
      password_element_ = control_elements[1].to<blink::WebInputElement>();
    } else {
      password_element_ = control_elements[0].to<blink::WebInputElement>();
    }

    UpdateOriginForHTML(test_case.html_form);
    if (test_case.does_trigger_autocomplete_on_fill) {
      // Prepare |fill_data_| to trigger autocomplete.
      fill_data_.is_possible_change_password_form =
          test_case.is_possible_change_password_form;
      fill_data_.username_field.name =
          ASCIIToUTF16(test_case.fill_data_username_field_name);
      fill_data_.password_field.name =
          ASCIIToUTF16(test_case.fill_data_password_field_name);
      fill_data_.additional_logins.clear();
      fill_data_.other_possible_usernames.clear();

      ClearUsernameAndPasswordFields();

      // Simulate the browser sending back the login info, it triggers the
      // autocomplete.
      SimulateOnFillPasswordForm(fill_data_);

      if (has_fillable_username) {
        SimulateSuggestionChoice(username_element_);
      } else {
        SimulateSuggestionChoice(password_element_);
      }

      // The username and password should now have been autocompleted.
      CheckTextFieldsDOMState(test_case.expected_username_suggestions,
                              test_case.expected_is_username_autofillable,
                              test_case.expected_password_suggestions,
                              test_case.expected_is_password_autofillable);
    }
  }
}

// The password manager autofills credentials, the user chooses another
// credentials option from a suggestion dropdown and then the user submits a
// form. This test verifies that the browser process receives submitted
// username/password from the renderer process.
TEST_F(PasswordAutofillAgentTest, RememberChosenUsernamePassword) {
  SimulateOnFillPasswordForm(fill_data_);
  SimulateSuggestionChoiceOfUsernameAndPassword(username_element_,
                                                ASCIIToUTF16(kBobUsername),
                                                ASCIIToUTF16(kBobPassword));

  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent sends to the browser selected
  // credentials.
  ExpectFormSubmittedWithUsernameAndPasswords(kBobUsername, kBobPassword, "");
}

// Tests that we can correctly suggest to autofill two forms without username
// fields.
TEST_F(PasswordAutofillAgentTest, ShowSuggestionForNonUsernameFieldForms) {
  LoadHTML(kTwoNoUsernameFormsHTML);
  fill_data_.username_field.name.clear();
  fill_data_.username_field.value.clear();
  UpdateOriginForHTML(kTwoNoUsernameFormsHTML);
  SimulateOnFillPasswordForm(fill_data_);

  SimulateElementClick("password1");
  CheckSuggestions(std::string(), false);
  SimulateElementClick("password2");
  CheckSuggestions(std::string(), false);
}

TEST_F(PasswordAutofillAgentTest,
       UsernameChangedAfterPasswordInput_InPageNavigation) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameChange("Bob");
  SimulatePasswordChange("mypassword");
  SimulateUsernameChange("Alice");

  // Hide form elements to simulate successful login.
  username_element_.setAttribute("style", "display:none;");
  password_element_.setAttribute("style", "display:none;");

  password_autofill_agent_->AJAXSucceeded();

  ExpectInPageNavigationWithUsernameAndPasswords("Alice", "mypassword", "");
}

TEST_F(PasswordAutofillAgentTest,
       UsernameChangedAfterPasswordInput_FormSubmitted) {
  SimulateUsernameChange("Bob");
  SimulatePasswordChange("mypassword");
  SimulateUsernameChange("Alice");

  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  ExpectFormSubmittedWithUsernameAndPasswords("Alice", "mypassword", "");
}

// Tests that a suggestion dropdown is shown on a password field even if a
// username field is present.
TEST_F(PasswordAutofillAgentTest, SuggestPasswordFieldSignInForm) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);

  // Simulate a user clicking on the password element. This should produce a
  // dropdown with suggestion of all available usernames.
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(password_element_, false);
  CheckSuggestions("", false);
}

// Tests that a suggestion dropdown is shown on each password field. But when a
// user chose one of the fields to autofill, a suggestion dropdown will be shown
// only on this field.
TEST_F(PasswordAutofillAgentTest, SuggestMultiplePasswordFields) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateOriginForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);

  // Simulate a user clicking on the password elements. This should produce
  // dropdowns with suggestion of all available usernames.
  SimulateElementClick("password");
  CheckSuggestions("", false);

  SimulateElementClick("newpassword");
  CheckSuggestions("", false);

  SimulateElementClick("confirmpassword");
  CheckSuggestions("", false);

  // The user chooses to autofill the current password field.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      password_element_, ASCIIToUTF16(kAliceUsername),
      ASCIIToUTF16(kAlicePassword)));

  // Simulate a user clicking on not autofilled password fields. This should
  // produce no suggestion dropdowns.
  fake_driver_.reset_show_pw_suggestions();
  SimulateElementClick("newpassword");
  SimulateElementClick("confirmpassword");
  EXPECT_FALSE(GetCalledShowPasswordSuggestions());

  // But when the user clicks on the autofilled password field again it should
  // still produce a suggestion dropdown.
  SimulateElementClick("password");
  CheckSuggestions("", false);
}

TEST_F(PasswordAutofillAgentTest, ShowAutofillSignaturesFlag) {
  const bool kFalseTrue[] = {false, true};
  for (bool show_signatures : kFalseTrue) {
    if (show_signatures)
      EnableShowAutofillSignatures();

    LoadHTML(kFormHTML);
    WebDocument document = GetMainFrame()->document();
    WebFormElement form_element =
        document.getElementById(WebString::fromASCII("LoginTestForm"))
            .to<WebFormElement>();
    ASSERT_FALSE(form_element.isNull());
    FormData form_data;
    ASSERT_TRUE(WebFormElementToFormData(
        form_element, blink::WebFormControlElement(), nullptr,
        form_util::EXTRACT_NONE, &form_data, nullptr));

    // Check form signature.
    WebString form_signature_attribute =
        WebString::fromASCII(kDebugAttributeForFormSignature);
    ASSERT_EQ(form_element.hasAttribute(form_signature_attribute),
              show_signatures);
    if (show_signatures) {
      EXPECT_EQ(form_element.getAttribute(form_signature_attribute),
                blink::WebString::fromUTF8(
                    base::Uint64ToString(CalculateFormSignature(form_data))));
    }

    // Check field signatures.
    WebString field_signature_attribute =
        WebString::fromASCII(kDebugAttributeForFieldSignature);
    blink::WebVector<blink::WebFormControlElement> control_elements;
    form_element.getFormControlElements(control_elements);
    for (size_t i = 0; i < control_elements.size(); ++i) {
      const blink::WebFormControlElement field_element = control_elements[i];
      bool expect_signature_for_field =
          show_signatures && form_util::IsAutofillableElement(field_element);
      ASSERT_EQ(field_element.hasAttribute(field_signature_attribute),
                expect_signature_for_field);
      if (expect_signature_for_field) {
        EXPECT_EQ(field_element.getAttribute(field_signature_attribute),
                  blink::WebString::fromUTF8(base::Uint64ToString(
                      CalculateFieldSignatureForField(form_data.fields[i]))));
      }
    }
  }
}

// Tests that a suggestion dropdown is shown even if JavaScripts updated field
// names.
TEST_F(PasswordAutofillAgentTest, SuggestWhenJavaScriptUpdatesFieldNames) {
  // Simulate that JavaScript updated field names.
  auto fill_data = fill_data_;
  fill_data.username_field.name += ASCIIToUTF16("1");
  fill_data.password_field.name += ASCIIToUTF16("1");
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data);

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);

  // Simulate a user clicking on the password element. This should produce a
  // dropdown with suggestion of all available usernames.
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(password_element_, false);
  CheckSuggestions("", false);
}

}  // namespace autofill
