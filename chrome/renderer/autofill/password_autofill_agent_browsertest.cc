// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_autofill_agent.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/renderer/autofill/fake_mojo_password_manager_driver.h"
#include "chrome/renderer/autofill/fake_password_generation_driver.h"
#include "chrome/renderer/autofill/password_generation_test_utils.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/renderer_id.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(OS_ANDROID)
#include "components/autofill/core/common/autofill_features.h"
#endif

#if defined(OS_WIN)
#include "third_party/blink/public/web/win/web_font_rendering.h"
#endif

using autofill::FormTracker;
using autofill::mojom::FocusedFieldType;
using autofill::mojom::PasswordFormFieldPredictionType;
using autofill::mojom::SubmissionIndicatorEvent;
using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebString;
using testing::_;
using testing::AtMost;
using testing::Eq;
using testing::Truly;

namespace {

// The name of the username/password element in the form.
const char kUsernameName[] = "username";
const char kPasswordName[] = "password";
const char kSearchField[] = "search";
const char kSocialMediaTextArea[] = "new_chirp";

const char kAliceUsername[] = "alice";
const char kAlicePassword[] = "password";
const char kBobUsername[] = "bob";
const char kBobPassword[] = "secret";
const char kCarolUsername[] = "Carol";
const char kCarolPassword[] = "test";
const char kCarolAlternateUsername[] = "RealCarolUsername";
const char kEmptyUsername[] = "";
const char kEmptyUsernamePassword[] = "empty";

const char kFormHTML[] =
    "<FORM id='LoginTestForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='random_field'/>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='password' id='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kSocialNetworkPostFormHTML[] =
    "<FORM id='SocialMediaPostForm' action='http://www.chirper.com'>"
    "  <TEXTAREA id='new_chirp'>"
    "  </TEXTAREA>"
    "  <INPUT type='submit' value='Chirp'/>"
    "</FORM>";

const char kSearchFieldHTML[] =
    "<FORM id='SearchFieldForm' action='http://www.gewgle.de'>"
    "  <INPUT type='search' id='search'/>"
    "  <INPUT type='submit' value='Chirp'/>"
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

const char kFormWithoutPasswordsHTML[] =
    "<FORM>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='text' id='random_field'/>"
    "</FORM>";

const char kNonVisibleFormHTML[] =
    "<head> <style> form {visibility: hidden;} </style> </head>"
    "<body>"
    "  <form>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

const char kNonDisplayedFormHTML[] =
    "<head> <style> form {display: none;} </style> </head>"
    "<body>"
    "  <form>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

const char kSignupFormHTML[] =
    "<FORM id='LoginTestForm' name='LoginTestForm' "
    "    action='http://www.bidule.com'>"
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

const char kDivWrappedFormHTML[] =
    "<DIV id='outer'>"
    "  <DIV id='inner'>"
    "    <FORM id='form' action='http://www.bidule.com'>"
    "      <INPUT type='text' id='username'/>"
    "      <INPUT type='password' id='password'/>"
    "    </FORM>"
    "  </DIV>"
    "</DIV>";

// Sets the "readonly" attribute of |element| to the value given by |read_only|.
void SetElementReadOnly(WebInputElement& element, bool read_only) {
  element.SetAttribute(WebString::FromUTF8("readonly"),
                       read_only ? WebString::FromUTF8("true") : WebString());
}

bool FormHasFieldWithValue(const autofill::FormData& form,
                           const std::string& value) {
  base::string16 value_16 = ASCIIToUTF16(value);
  for (const auto& field : form.fields) {
    if (field.value == value_16)
      return true;
    if (field.typed_value == value_16)
      return true;
  }
  return false;
}

enum PasswordFormSourceType {
  PasswordFormSubmitted,
  PasswordFormSameDocumentNavigation,
};

enum class FieldChangeSource { USER, AUTOFILL, USER_AUTOFILL };

}  // namespace

namespace autofill {

class PasswordAutofillAgentTest : public ChromeRenderViewTest {
 public:
  PasswordAutofillAgentTest() {}

  // Simulates the fill password form message being sent to the renderer.
  // We use that so we don't have to make RenderView::OnFillPasswordForm()
  // protected.
  void SimulateOnFillPasswordForm(const PasswordFormFillData& fill_data) {
    password_autofill_agent_->FillPasswordForm(fill_data);
  }

  void SendVisiblePasswordForms() {
    static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
        ->DidFinishLoad();
  }

  void SetUp() override {
    ChromeRenderViewTest::SetUp();

#if defined(OS_WIN)
    // Autofill uses the system font to render suggestion previews. On Windows
    // an extra step is required to ensure that the system font is configured.
    blink::WebFontRendering::SetMenuFontMetrics(
        base::ASCIIToUTF16("Arial").c_str(), 12);
#endif

    // TODO(crbug/862989): Remove workaround preventing non-test classes to bind
    // fake_driver_ or fake_pw_client_.
    password_autofill_agent_->GetPasswordManagerDriver();
    password_generation_->RequestPasswordManagerClientForTesting();
    base::RunLoop().RunUntilIdle();  // Executes binding the interfaces.
    // Reject all requests to bind driver/client to anything but the test class:
    view_->GetMainRenderFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::PasswordGenerationDriver::Name_,
            base::BindRepeating([](mojo::ScopedInterfaceEndpointHandle handle) {
              handle.reset();
            }));
    view_->GetMainRenderFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::PasswordManagerDriver::Name_,
            base::BindRepeating([](mojo::ScopedInterfaceEndpointHandle handle) {
              handle.reset();
            }));

    // Add a preferred login and an additional login to the FillData.
    username1_ = ASCIIToUTF16(kAliceUsername);
    password1_ = ASCIIToUTF16(kAlicePassword);
    username2_ = ASCIIToUTF16(kBobUsername);
    password2_ = ASCIIToUTF16(kBobPassword);
    username3_ = ASCIIToUTF16(kCarolUsername);
    password3_ = ASCIIToUTF16(kCarolPassword);
    alternate_username3_ = ASCIIToUTF16(kCarolAlternateUsername);
    username4_ = ASCIIToUTF16(kEmptyUsername);
    password4_ = ASCIIToUTF16(kEmptyUsernamePassword);

    FormFieldData username_field;
    username_field.name = ASCIIToUTF16(kUsernameName);
    username_field.value = username1_;
    fill_data_.username_field = username_field;

    FormFieldData password_field;
    password_field.name = ASCIIToUTF16(kPasswordName);
    password_field.value = password1_;
    password_field.form_control_type = "password";
    fill_data_.password_field = password_field;

    PasswordAndMetadata password2;
    password2.password = password2_;
    password2.username = username2_;
    fill_data_.additional_logins.push_back(std::move(password2));
    PasswordAndMetadata password3;
    password3.password = password3_;
    password3.username = username3_;
    fill_data_.additional_logins.push_back(std::move(password3));
    PasswordAndMetadata password4;
    password3.password = password4_;
    password4.username = username4_;
    fill_data_.additional_logins.push_back(std::move(password4));

    // We need to set the origin so it matches the frame URL and the action so
    // it matches the form action, otherwise we won't autocomplete.
    UpdateOriginForHTML(kFormHTML);
    fill_data_.action = GURL("http://www.bidule.com");

    LoadHTML(kFormHTML);

    // Necessary for SimulateElementClick() to work correctly.
    GetWebWidget()->Resize(blink::WebSize(500, 500));
    GetWebWidget()->SetFocus(true);

    // Now retrieve the input elements so the test can access them.
    UpdateUsernameAndPasswordElements();
  }

  void TearDown() override {
    username_element_.Reset();
    password_element_.Reset();
    ChromeRenderViewTest::TearDown();
  }

  void RegisterMainFrameRemoteInterfaces() override {
    // Because the test cases only involve the main frame in this test,
    // the fake password client and the fake driver is only used on main frame.
    blink::AssociatedInterfaceProvider* remote_associated_interfaces =
        view_->GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
    remote_associated_interfaces->OverrideBinderForTesting(
        mojom::PasswordGenerationDriver::Name_,
        base::BindRepeating(
            &PasswordAutofillAgentTest::BindPasswordManagerClient,
            base::Unretained(this)));
    remote_associated_interfaces->OverrideBinderForTesting(
        mojom::PasswordManagerDriver::Name_,
        base::BindRepeating(
            &PasswordAutofillAgentTest::BindPasswordManagerDriver,
            base::Unretained(this)));
  }

  void FocusElement(std::string element_id) {
    std::string script =
        "document.getElementById('" + element_id + "').focus()";
    ExecuteJavaScriptForTests(script.c_str());
    GetMainFrame()->AutofillClient()->DidCompleteFocusChangeInFrame();
  }

  void SetFillOnAccountSelect() {
    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kFillOnAccountSelect);
  }

#if defined(OS_ANDROID)
  void EnableTouchToFillFeature() {
    scoped_feature_list_.InitAndEnableFeature(
        autofill::features::kTouchToFillAndroid);
  }
#endif

  void EnableShowAutofillSignatures() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kShowAutofillSignatures);
  }

  void UpdateOriginForHTML(const std::string& html) {
    std::string origin = "data:text/html;charset=utf-8," + html;
    fill_data_.origin = GURL(origin);
  }

  void UpdateRendererIDs() {
    fill_data_.has_renderer_ids = true;
    if (!username_element_.IsNull()) {
      fill_data_.username_field.unique_renderer_id = autofill::FieldRendererId(
          username_element_.UniqueRendererFormControlId());
    }
    ASSERT_FALSE(password_element_.IsNull());
    fill_data_.password_field.unique_renderer_id = autofill::FieldRendererId(
        password_element_.UniqueRendererFormControlId());
    WebFormElement form = password_element_.Form();
    fill_data_.form_renderer_id =
        form.IsNull() ? autofill::FormRendererId()
                      : autofill::FormRendererId(form.UniqueRendererFormId());
  }

  void UpdateUsernameAndPasswordElements() {
    WebDocument document = GetMainFrame()->GetDocument();
    WebElement element =
        document.GetElementById(WebString::FromUTF8(kUsernameName));
    ASSERT_FALSE(element.IsNull());
    username_element_ = element.To<WebInputElement>();
    element = document.GetElementById(WebString::FromUTF8(kPasswordName));
    ASSERT_FALSE(element.IsNull());
    password_element_ = element.To<WebInputElement>();
    UpdateRendererIDs();
  }

  void UpdateOnlyPasswordElement() {
    WebDocument document = GetMainFrame()->GetDocument();
    WebElement element =
        document.GetElementById(WebString::FromUTF8(kPasswordName));
    ASSERT_FALSE(element.IsNull());
    password_element_ = element.To<WebInputElement>();
    username_element_.Reset();
    UpdateRendererIDs();
  }

  WebInputElement GetInputElementByID(const std::string& id) {
    WebDocument document = GetMainFrame()->GetDocument();
    WebElement element =
        document.GetElementById(WebString::FromUTF8(id.c_str()));
    return element.To<WebInputElement>();
  }

  void ClearUsernameAndPasswordFields() {
    username_element_.SetValue(WebString());
    username_element_.SetSuggestedValue(WebString());
    username_element_.SetAutofillState(WebAutofillState::kNotFilled);
    password_element_.SetValue(WebString());
    password_element_.SetSuggestedValue(WebString());
    password_element_.SetAutofillState(WebAutofillState::kNotFilled);
    UpdateRendererIDs();
  }

  void SimulateSuggestionChoice(WebInputElement& username_input) {
    base::string16 username(ASCIIToUTF16(kAliceUsername));
    base::string16 password(ASCIIToUTF16(kAlicePassword));
    SimulateSuggestionChoiceOfUsernameAndPassword(username_input, username,
                                                  password);
  }

  void SimulateSuggestionChoiceOfUsernameAndPassword(
      WebInputElement& input,
      const base::string16& username,
      const base::string16& password) {
    // This call is necessary to setup the autofill agent appropriate for the
    // user selection; simulates the menu actually popping up.
    SimulatePointClick(gfx::Point(1, 1));
    autofill_agent_->FormControlElementClicked(input, false);

    autofill_agent_->FillPasswordSuggestion(username, password);
  }

  void SimulateUsernameTyping(const std::string& username) {
    SimulatePointClick(gfx::Point(1, 1));
    SimulateUserInputChangeForElement(&username_element_, username);
  }

  void SimulatePasswordTyping(const std::string& password) {
    SimulateUserInputChangeForElement(&password_element_, password);
  }

  void SimulateUsernameFieldAutofill(const std::string& text) {
    // Simulate set |username_element_| in focus.
    static_cast<content::RenderFrameObserver*>(autofill_agent_)
        ->FocusedElementChanged(username_element_);
    // Fill focused element (i.e. |username_element_|).
    autofill_agent_->FillFieldWithValue(ASCIIToUTF16(text));
  }

  void SimulateUsernameFieldChange(FieldChangeSource change_source) {
    switch (change_source) {
      case FieldChangeSource::USER:
        SimulateUsernameTyping("Alice");
        break;
      case FieldChangeSource::AUTOFILL:
        SimulateUsernameFieldAutofill("Alice");
        break;
      case FieldChangeSource::USER_AUTOFILL:
        SimulateUsernameTyping("A");
        SimulateUsernameFieldAutofill("Alice");
        break;
    }
  }

  void CheckTextFieldsStateForElements(const WebInputElement& username_element,
                                       const std::string& username,
                                       bool username_autofilled,
                                       const WebInputElement& password_element,
                                       const std::string& password,
                                       bool password_autofilled,
                                       bool check_suggested_username,
                                       bool check_suggested_password) {
    EXPECT_EQ(username, check_suggested_username
                            ? username_element.SuggestedValue().Utf8()
                            : username_element.Value().Utf8())
        << "check_suggested_username == " << check_suggested_username;
    EXPECT_EQ(username_autofilled, username_element.IsAutofilled());

    EXPECT_EQ(password, check_suggested_password
                            ? password_element.SuggestedValue().Utf8()
                            : password_element.Value().Utf8())
        << "check_suggested_password == " << check_suggested_password;
    EXPECT_EQ(password_autofilled, password_element.IsAutofilled());
  }

  // Checks the DOM-accessible value of the username element and the
  // *suggested* value of the password element.
  void CheckUsernameDOMStatePasswordSuggestedState(const std::string& username,
                                                   bool username_autofilled,
                                                   const std::string& password,
                                                   bool password_autofilled) {
    CheckTextFieldsStateForElements(
        username_element_, username, username_autofilled, password_element_,
        password, password_autofilled, false /* check_suggested_username */,
        true /* check_suggested_password */);
  }

  // Checks the DOM-accessible value of the username element and the
  // DOM-accessible value of the password element.
  void CheckTextFieldsDOMState(const std::string& username,
                               bool username_autofilled,
                               const std::string& password,
                               bool password_autofilled) {
    CheckTextFieldsStateForElements(
        username_element_, username, username_autofilled, password_element_,
        password, password_autofilled, false /* check_suggested_username */,
        false /* check_suggested_password */);
  }

  // Checks the suggested values of the |username| and |password| elements.
  void CheckTextFieldsSuggestedState(const std::string& username,
                                     bool username_autofilled,
                                     const std::string& password,
                                     bool password_autofilled) {
    CheckTextFieldsStateForElements(
        username_element_, username, username_autofilled, password_element_,
        password, password_autofilled, true /* check_suggested_username */,
        true /* check_suggested_password */);
  }

  void ResetFieldState(
      WebInputElement* element,
      const std::string& value = std::string(),
      blink::WebAutofillState is_autofilled = WebAutofillState::kNotFilled) {
    element->SetValue(WebString::FromUTF8(value));
    element->SetSuggestedValue(WebString());
    element->SetAutofillState(is_autofilled);
    element->SetSelectionRange(value.size(), value.size());
  }

  void CheckUsernameSelection(int start, int end) {
    EXPECT_EQ(start, username_element_.SelectionStart());
    EXPECT_EQ(end, username_element_.SelectionEnd());
  }

  // Checks the message sent to PasswordAutofillManager to build the suggestion
  // list. |username| is the expected username field value, and |show_all| is
  // the expected flag for the PasswordAutofillManager, whether to show all
  // suggestions, or only those starting with |username|.
  void CheckSuggestions(const std::string& username, bool show_all) {
    auto show_all_matches = [show_all](int options) {
      return (show_all == ((options & autofill::SHOW_ALL) != 0)) ||
             (show_all == ((options & autofill::IS_PASSWORD_FIELD) != 0));
    };

    EXPECT_CALL(fake_driver_,
                ShowPasswordSuggestions(_, Eq(ASCIIToUTF16(username)),
                                        Truly(show_all_matches), _));
    base::RunLoop().RunUntilIdle();
  }

  void ExpectFieldPropertiesMasks(
      PasswordFormSourceType expected_type,
      const std::map<base::string16, FieldPropertiesMask>&
          expected_properties_masks,
      autofill::mojom::SubmissionIndicatorEvent expected_submission_event) {
    base::RunLoop().RunUntilIdle();
    autofill::FormData form_data;
    if (expected_type == PasswordFormSubmitted) {
      ASSERT_TRUE(fake_driver_.called_password_form_submitted());
      ASSERT_TRUE(static_cast<bool>(fake_driver_.form_data_submitted()));
      form_data = *(fake_driver_.form_data_submitted());
    } else {
      ASSERT_EQ(PasswordFormSameDocumentNavigation, expected_type);
      ASSERT_TRUE(fake_driver_.called_same_document_navigation());
      ASSERT_TRUE(static_cast<bool>(fake_driver_.form_data_maybe_submitted()));
      form_data = *(fake_driver_.form_data_maybe_submitted());
      EXPECT_EQ(expected_submission_event, form_data.submission_event);
    }

    size_t unchecked_masks = expected_properties_masks.size();
    for (const FormFieldData& field : form_data.fields) {
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

  autofill::FormRendererId GetFormUniqueRendererId(const WebString& form_id) {
    WebLocalFrame* frame = GetMainFrame();
    if (!frame)
      return autofill::FormRendererId();
    WebFormElement web_form =
        frame->GetDocument().GetElementById(form_id).To<WebFormElement>();
    return autofill::FormRendererId(web_form.UniqueRendererFormId());
  }

  void ExpectFormDataWithUsernameAndPasswordsAndEvent(
      const autofill::FormData& form_data,
      autofill::FormRendererId form_renderer_id,
      const std::string& username_value,
      const std::string& password_value,
      const std::string& new_password_value,
      SubmissionIndicatorEvent event) {
    EXPECT_EQ(form_renderer_id, form_data.unique_renderer_id);
    EXPECT_TRUE(FormHasFieldWithValue(form_data, username_value));
    EXPECT_TRUE(FormHasFieldWithValue(form_data, password_value));
    EXPECT_TRUE(FormHasFieldWithValue(form_data, new_password_value));
    EXPECT_EQ(form_data.submission_event, event);
  }

  void ExpectFormSubmittedWithUsernameAndPasswords(
      autofill::FormRendererId form_renderer_id,
      const std::string& username_value,
      const std::string& password_value,
      const std::string& new_password_value) {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(fake_driver_.called_password_form_submitted());
    ASSERT_TRUE(static_cast<bool>(fake_driver_.form_data_submitted()));
    ExpectFormDataWithUsernameAndPasswordsAndEvent(
        *(fake_driver_.form_data_submitted()), form_renderer_id, username_value,
        password_value, new_password_value,
        SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
  }

  void ExpectSameDocumentNavigationWithUsernameAndPasswords(
      autofill::FormRendererId form_renderer_id,
      const std::string& username_value,
      const std::string& password_value,
      const std::string& new_password_value,
      SubmissionIndicatorEvent event) {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(fake_driver_.called_same_document_navigation());
    ASSERT_TRUE(static_cast<bool>(fake_driver_.form_data_maybe_submitted()));
    ExpectFormDataWithUsernameAndPasswordsAndEvent(
        *(fake_driver_.form_data_maybe_submitted()), form_renderer_id,
        username_value, password_value, new_password_value, event);
  }

  void CheckIfEventsAreCalled(const std::vector<base::string16>& checkers,
                              bool expected) {
    for (const base::string16& variable : checkers) {
      int value;
      EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(variable, &value))
          << variable;
      EXPECT_EQ(expected, value == 1) << variable;
    }
  }

  void BindPasswordManagerDriver(mojo::ScopedInterfaceEndpointHandle handle) {
    fake_driver_.BindReceiver(
        mojo::PendingAssociatedReceiver<mojom::PasswordManagerDriver>(
            std::move(handle)));
  }

  void BindPasswordManagerClient(mojo::ScopedInterfaceEndpointHandle handle) {
    fake_pw_client_.BindReceiver(
        mojo::PendingAssociatedReceiver<mojom::PasswordGenerationDriver>(
            std::move(handle)));
  }

  void SaveAndSubmitForm() { SaveAndSubmitForm(username_element_.Form()); }

  void SaveAndSubmitForm(const WebFormElement& form_element) {
    FormTracker* tracker = autofill_agent_->form_tracker_for_testing();
    static_cast<content::RenderFrameObserver*>(tracker)->WillSendSubmitEvent(
        form_element);
    static_cast<content::RenderFrameObserver*>(tracker)->WillSubmitForm(
        form_element);
  }

  void CheckFirstFillingResult(FillingResult result) {
    histogram_tester_.ExpectUniqueSample(
        "PasswordManager.FirstRendererFillingResult", result, 1);
  }

  void SubmitForm() {
    FormTracker* tracker = autofill_agent_->form_tracker_for_testing();
    static_cast<content::RenderFrameObserver*>(tracker)->WillSubmitForm(
        username_element_.Form());
  }

  void FireAjaxSucceeded() {
    FormTracker* tracker = autofill_agent_->form_tracker_for_testing();
    tracker->AjaxSucceeded();
  }

  void FireDidCommitProvisionalLoad() {
    FormTracker* tracker = autofill_agent_->form_tracker_for_testing();
    static_cast<content::RenderFrameObserver*>(tracker)
        ->DidCommitProvisionalLoad(true, ui::PAGE_TRANSITION_LINK);
  }

  void ClearField(FormFieldData* field) {
    field->unique_renderer_id = autofill::FieldRendererId();
    field->value.clear();
  }

  FakeMojoPasswordManagerDriver fake_driver_;
  testing::NiceMock<FakePasswordGenerationDriver> fake_pw_client_;

  base::string16 username1_;
  base::string16 username2_;
  base::string16 username3_;
  base::string16 username4_;
  base::string16 password1_;
  base::string16 password2_;
  base::string16 password3_;
  base::string16 alternate_username3_;
  base::string16 password4_;
  PasswordFormFillData fill_data_;

  WebInputElement username_element_;
  WebInputElement password_element_;
  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillAgentTest);
};

// Tests that the password login is autocompleted as expected when the browser
// sends back the password info.
TEST_F(PasswordAutofillAgentTest, InitialAutocomplete) {
  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  CheckFirstFillingResult(FillingResult::kSuccess);
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
  UpdateUsernameAndPasswordElements();

  // Set the expected form origin and action URLs.
  UpdateOriginForHTML(kEmptyActionFormHTML);
  fill_data_.action = GURL();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that if a password is marked as readonly, neither field is autofilled
// on page load.
TEST_F(PasswordAutofillAgentTest, NoInitialAutocompleteForReadOnlyPassword) {
  SetElementReadOnly(password_element_, true);

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState(std::string(), false, std::string(), false);

  CheckFirstFillingResult(FillingResult::kPasswordElementIsNotAutocompleteable);
}

// Can still fill a password field if the username is set to a value that
// matches.
TEST_F(PasswordAutofillAgentTest,
       AutocompletePasswordForReadonlyUsernameMatched) {
  username_element_.SetValue(WebString::FromUTF16(username3_));
  SetElementReadOnly(username_element_, true);

  // Filled even though username is not the preferred match.
  SimulateOnFillPasswordForm(fill_data_);
  CheckUsernameDOMStatePasswordSuggestedState(UTF16ToUTF8(username3_), false,
                                              UTF16ToUTF8(password3_), true);

  CheckFirstFillingResult(FillingResult::kSuccess);
}

// Fill username and password fields when username field contains a prefilled
// value that matches the list of known possible prefilled values usually used
// as placeholders.
TEST_F(PasswordAutofillAgentTest, AutocompleteForPrefilledUsernameValue) {
  // Set the username element to a value from the prefilled values list.
  // Comparison should be insensitive to leading and trailing whitespaces.
  username_element_.SetValue(
      WebString::FromUTF16(base::UTF8ToUTF16(" User Name ")));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should both have suggested values.
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  // Simulate a user click so that the password field's real value is filled.
  SimulateElementClick(kUsernameName);

  // The username and password should have been autocompleted.
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PrefilledUsernameFillOutcome",
      PrefilledUsernameFillOutcome::kPrefilledPlaceholderUsernameOverridden, 1);

  CheckFirstFillingResult(FillingResult::kSuccess);
}

// Tests that if filling is invoked twice for the same autofill agent the
// prefilled username and first filling metrics are only logged once.
TEST_F(PasswordAutofillAgentTest, MetricsOnlyLoggedOnce) {
  // Set the username element to a value from the prefilled values list.
  // Comparison should be insensitive to leading and trailing whitespaces.
  username_element_.SetValue(
      WebString::FromUTF16(base::UTF8ToUTF16(" User Name ")));

  // Simulate the browser sending back the login info multiple tims.
  // This triggers the autocomplete.
  SimulateOnFillPasswordForm(fill_data_);
  SimulateOnFillPasswordForm(fill_data_);

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PrefilledUsernameFillOutcome",
      PrefilledUsernameFillOutcome::kPrefilledPlaceholderUsernameOverridden, 1);

  CheckFirstFillingResult(FillingResult::kSuccess);
}

// Fill a password field if the stored username is a prefix of username in
// read-only field.
TEST_F(PasswordAutofillAgentTest,
       AutocompletePasswordForReadonlyUsernamePrefixMatched) {
  base::string16 username_at = username3_ + base::UTF8ToUTF16("@example.com");
  username_element_.SetValue(WebString::FromUTF16(username_at));
  SetElementReadOnly(username_element_, true);

  // Filled even though the username in the form is only a proper prefix of the
  // stored username.
  SimulateOnFillPasswordForm(fill_data_);
  CheckUsernameDOMStatePasswordSuggestedState(UTF16ToUTF8(username_at), false,
                                              UTF16ToUTF8(password3_), true);
}

// Credentials are sent to the renderer even for sign-up forms as these may be
// eligible for filling via manual fall back. In this case, the username_field
// and password_field are not set. This test verifies that no failures are
// recorded in PasswordManager.FirstRendererFillingResult.
TEST_F(PasswordAutofillAgentTest, NoFillingOnSignupForm_NoMetrics) {
  LoadHTML(kSignupFormHTML);

  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element =
      document.GetElementById(WebString::FromUTF8("random_info"));
  ASSERT_FALSE(element.IsNull());
  username_element_ = element.To<WebInputElement>();

  fill_data_.username_field.unique_renderer_id = autofill::FieldRendererId();
  fill_data_.password_field.unique_renderer_id = autofill::FieldRendererId();

  WebFormElement form_element =
      document.GetElementById("LoginTestForm").To<WebFormElement>();
  fill_data_.form_renderer_id =
      autofill::FormRendererId(form_element.UniqueRendererFormId());

  SimulateOnFillPasswordForm(fill_data_);
  histogram_tester_.ExpectTotalCount(
      "PasswordManager.FirstRendererFillingResult", 0);
}

// Do not fill a password field if the stored username is a prefix without @
// of username in read-only field.
TEST_F(PasswordAutofillAgentTest,
       DontAutocompletePasswordForReadonlyUsernamePrefixMatched) {
  base::string16 prefilled_username =
      username3_ + base::UTF8ToUTF16("example.com");
  username_element_.SetValue(WebString::FromUTF16(prefilled_username));
  SetElementReadOnly(username_element_, true);

  // Filled even though the username in the form is only a proper prefix of the
  // stored username.
  SimulateOnFillPasswordForm(fill_data_);
  CheckUsernameDOMStatePasswordSuggestedState(UTF16ToUTF8(prefilled_username),
                                              false, std::string(), false);

  CheckFirstFillingResult(
      FillingResult::kUsernamePrefilledWithIncompatibleValue);
}

// Do not fill a password field if the field isn't readonly despite the stored
// username is a prefix without @ of username in read-only field.
TEST_F(
    PasswordAutofillAgentTest,
    DontAutocompletePasswordForNotReadonlyUsernameFieldEvenWhenPrefixMatched) {
  base::string16 prefilled_username =
      username3_ + base::UTF8ToUTF16("@example.com");
  username_element_.SetValue(WebString::FromUTF16(prefilled_username));

  // Filled even though the username in the form is only a proper prefix of the
  // stored username.
  SimulateOnFillPasswordForm(fill_data_);
  CheckUsernameDOMStatePasswordSuggestedState(UTF16ToUTF8(prefilled_username),
                                              false, std::string(), false);
}

// If a username field is empty and readonly, don't autofill.
TEST_F(PasswordAutofillAgentTest,
       NoAutocompletePasswordForReadonlyUsernameUnmatched) {
  username_element_.SetValue(WebString::FromUTF8(""));
  SetElementReadOnly(username_element_, true);

  SimulateOnFillPasswordForm(fill_data_);
  CheckUsernameDOMStatePasswordSuggestedState(std::string(), false,
                                              std::string(), false);

  CheckFirstFillingResult(FillingResult::kFoundNoPasswordForUsername);
}

// Tests that having a non-matching username precludes the autocomplete.
TEST_F(PasswordAutofillAgentTest, NoAutocompleteForFilledFieldUnmatched) {
  username_element_.SetValue(WebString::FromUTF8("bogus"));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckUsernameDOMStatePasswordSuggestedState("bogus", false, std::string(),
                                              false);

  CheckFirstFillingResult(
      FillingResult::kUsernamePrefilledWithIncompatibleValue);
}

// Don't try to complete a prefilled value that is a partial match
// to a username if the prefilled value isn't on the list of known values
// used as placeholders.
TEST_F(PasswordAutofillAgentTest, NoPartialMatchForPrefilledUsername) {
  username_element_.SetValue(WebString::FromUTF8("ali"));

  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState("", false, std::string(), false);
  CheckUsernameDOMStatePasswordSuggestedState("ali", false, std::string(),
                                              false);

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PrefilledUsernameFillOutcome",
      autofill::PrefilledUsernameFillOutcome::kPrefilledUsernameNotOverridden,
      1);
}

TEST_F(PasswordAutofillAgentTest, InputWithNoForms) {
  const char kNoFormInputs[] =
      "<input type='text' id='username'/>"
      "<input type='password' id='password'/>";
  LoadHTML(kNoFormInputs);

  SimulateOnFillPasswordForm(fill_data_);

  // Input elements that aren't in a <form> won't autofill.
  CheckTextFieldsSuggestedState(std::string(), false, std::string(), false);
}

// Tests that having a matching username precludes the autofill.
TEST_F(PasswordAutofillAgentTest, InitialAutocompleteForMatchingFilledField) {
  username_element_.SetValue(WebString::FromUTF8(kAliceUsername));

  // Simulate the browser sending back the login info, it triggers the
  // autofill.
  SimulateOnFillPasswordForm(fill_data_);

  // The password should have been autofilled, but the username field should
  // have been left alone, since it contained the correct value already.
  CheckUsernameDOMStatePasswordSuggestedState(kAliceUsername, false,
                                              kAlicePassword, true);

  CheckFirstFillingResult(FillingResult::kSuccess);
}

TEST_F(PasswordAutofillAgentTest, PasswordNotClearedOnEdit) {
  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate the user changing the username to some unknown username.
  SimulateUsernameTyping("alicia");

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
  CheckTextFieldsSuggestedState(std::string(), false, std::string(), false);

  SimulateUsernameTyping(kAliceUsername);
  // Change focus in between to make sure blur events don't trigger filling.
  SetFocused(password_element_);
  SetFocused(username_element_);
  // No autocomplete should happen when text is entered in the username.
  CheckUsernameDOMStatePasswordSuggestedState(kAliceUsername, false,
                                              std::string(), false);

  CheckFirstFillingResult(FillingResult::kWaitForUsername);
}

TEST_F(PasswordAutofillAgentTest, IsWebElementVisibleTest) {
  blink::WebLocalFrame* frame;

  LoadHTML(kVisibleFormWithNoUsernameHTML);
  frame = GetMainFrame();
  blink::WebVector<WebFormElement> forms = frame->GetDocument().Forms();
  ASSERT_EQ(1u, forms.size());
  blink::WebVector<blink::WebFormControlElement> web_control_elements =
      forms[0].GetFormControlElements();
  ASSERT_EQ(1u, web_control_elements.size());
  EXPECT_TRUE(form_util::IsWebElementVisible(web_control_elements[0]));

  LoadHTML(kNonVisibleFormHTML);
  frame = GetMainFrame();
  forms = frame->GetDocument().Forms();
  ASSERT_EQ(1u, forms.size());
  web_control_elements = forms[0].GetFormControlElements();
  ASSERT_EQ(1u, web_control_elements.size());
  EXPECT_FALSE(form_util::IsWebElementVisible(web_control_elements[0]));

  LoadHTML(kNonDisplayedFormHTML);
  frame = GetMainFrame();
  forms = frame->GetDocument().Forms();
  ASSERT_EQ(1u, forms.size());
  web_control_elements = forms[0].GetFormControlElements();
  ASSERT_EQ(1u, web_control_elements.size());
  EXPECT_FALSE(form_util::IsWebElementVisible(web_control_elements[0]));
}

TEST_F(PasswordAutofillAgentTest,
       SendPasswordFormsTest_VisibleFormWithNoUsername) {
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_FALSE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_EmptyForm) {
  base::RunLoop().RunUntilIdle();
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kEmptyFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_password_forms_parsed());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_TRUE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_FormWithoutPasswords) {
  base::RunLoop().RunUntilIdle();
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kFormWithoutPasswordsHTML);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_driver_.called_password_forms_parsed());

  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_TRUE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest,
       SendPasswordFormsTest_UndetectedPasswordField) {
  base::RunLoop().RunUntilIdle();
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kFormWithoutPasswordsHTML);
  // Emulate that a password field appears but we don't detect that.
  std::string script =
      "document.getElementById('random_field').type = 'password';";
  ExecuteJavaScriptForTests(script.c_str());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_password_forms_parsed());

  // When the user clicks on the field, a request to the store will be sent.
  EXPECT_TRUE(SimulateElementClick("random_field"));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());

  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_TRUE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_NonDisplayedForm) {
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kNonDisplayedFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_TRUE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_NonVisibleForm) {
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kNonVisibleFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_TRUE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_PasswordChangeForm) {
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kPasswordChangeFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_FALSE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest,
       SendPasswordFormsTest_CannotCreatePasswordForm) {
  // This test checks that a request to the store is sent even if it is a credit
  // card form.
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kCreditCardFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());
  ASSERT_TRUE(fake_driver_.form_data_rendered());
  EXPECT_FALSE(fake_driver_.form_data_rendered()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_ReloadTab) {
  // PasswordAutofillAgent::sent_request_to_store_ disables duplicate requests
  // to the store. This test checks that new request will be sent if the frame
  // has been reloaded.
  fake_driver_.reset_password_forms_calls();
  LoadHTML(kNonVisibleFormHTML);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());

  fake_driver_.reset_password_forms_calls();
  std::string url_string = "data:text/html;charset=utf-8,";
  url_string.append(kNonVisibleFormHTML);
  Reload(GURL(url_string));
  EXPECT_TRUE(fake_driver_.called_password_forms_parsed());
  ASSERT_TRUE(fake_driver_.form_data_parsed());
  EXPECT_FALSE(fake_driver_.form_data_parsed()->empty());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_Redirection) {
  base::RunLoop().RunUntilIdle();

  fake_driver_.reset_password_forms_calls();
  LoadHTML(kEmptyWebpage);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_password_forms_rendered());

  fake_driver_.reset_password_forms_calls();
  LoadHTML(kRedirectionWebpage);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_password_forms_rendered());

  fake_driver_.reset_password_forms_calls();
  LoadHTML(kSimpleWebpage);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_driver_.called_password_forms_rendered());

  fake_driver_.reset_password_forms_calls();
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
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  // However, it should only have completed with the suggested value, as tested
  // above, and it should not have completed into the DOM accessible value for
  // the password field.
  CheckTextFieldsDOMState(std::string(), true, std::string(), true);

  // Simulate a user click so that the password field's real value is filled.
  SimulateElementClick(kUsernameName);
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// Verfies that a DOM-activated UI event will not cause an autofill.
TEST_F(PasswordAutofillAgentTest, NoDOMActivationTest) {
  // Trigger the initial autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  ExecuteJavaScriptForTests(kJavaScriptClick);
  CheckTextFieldsDOMState("", true, "", true);
}

// Verifies that password autofill triggers events in JavaScript for forms that
// are filled on page load.
TEST_F(PasswordAutofillAgentTest,
       PasswordAutofillTriggersOnChangeEventsOnLoad) {
  std::vector<base::string16> username_event_checkers;
  std::vector<base::string16> password_event_checkers;
  std::string events_registration_script =
      CreateScriptToRegisterListeners(kUsernameName, &username_event_checkers) +
      CreateScriptToRegisterListeners(kPasswordName, &password_event_checkers);
  std::string html = std::string(kFormHTML) + events_registration_script;
  LoadHTML(html.c_str());
  UpdateOriginForHTML(html);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted...
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
  // ... but since there hasn't been a user gesture yet, the autocompleted
  // username and password should only be visible to the user.
  CheckTextFieldsDOMState(std::string(), true, std::string(), true);

  // JavaScript events shouldn't have been triggered for the username and the
  // password yet.
  CheckIfEventsAreCalled(username_event_checkers, false);
  CheckIfEventsAreCalled(password_event_checkers, false);

  // Simulate a user click so that the password field's real value is filled.
  SimulateElementClick(kUsernameName);
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  // Now, JavaScript events should have been triggered.
  CheckIfEventsAreCalled(username_event_checkers, true);
  CheckIfEventsAreCalled(password_event_checkers, true);
}

// Verifies that password autofill triggers events in JavaScript for forms that
// are filled after page load.
TEST_F(PasswordAutofillAgentTest,
       PasswordAutofillTriggersOnChangeEventsWaitForUsername) {
  std::vector<base::string16> event_checkers;
  std::string events_registration_script =
      CreateScriptToRegisterListeners(kUsernameName, &event_checkers) +
      CreateScriptToRegisterListeners(kPasswordName, &event_checkers);
  std::string html = std::string(kFormHTML) + events_registration_script;
  LoadHTML(html.c_str());
  UpdateOriginForHTML(html);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should not yet have been autocompleted.
  CheckTextFieldsSuggestedState(std::string(), false, std::string(), false);

  // Simulate a click just to force a user gesture, since the username value is
  // set directly.
  SimulateElementClick(kUsernameName);

  // Simulate the user entering the first letter of their username and selecting
  // the matching autofill from the dropdown.
  SimulateUsernameTyping("a");
  // Since the username element has focus, blur event will be not triggered.
  base::Erase(event_checkers, ASCIIToUTF16("username_blur_event"));
  SimulateSuggestionChoice(username_element_);

  // The username and password should now have been autocompleted.
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  // JavaScript events should have been triggered both for the username and for
  // the password.
  CheckIfEventsAreCalled(event_checkers, true);
}

// Tests that |FillSuggestion| properly fills the username and password.
TEST_F(PasswordAutofillAgentTest, FillSuggestion) {
  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  for (const auto& selected_element : {username_element_, password_element_}) {
    // Neither field should be autocompleted.
    CheckTextFieldsDOMState(std::string(), false, std::string(), false);

    // If the password field is not autocompletable, it should not be affected.
    SetElementReadOnly(password_element_, true);
    EXPECT_FALSE(password_autofill_agent_->FillSuggestion(
        selected_element, ASCIIToUTF16(kAliceUsername),
        ASCIIToUTF16(kAlicePassword)));
    CheckTextFieldsDOMState(std::string(), false, std::string(), false);
    SetElementReadOnly(password_element_, false);

    // After filling with the suggestion, both fields should be autocompleted.
    EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
        selected_element, ASCIIToUTF16(kAliceUsername),
        ASCIIToUTF16(kAlicePassword)));
    CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
    int username_length = strlen(kAliceUsername);
    CheckUsernameSelection(username_length, username_length);

    // Try Filling with a suggestion with password different from the one that
    // was initially sent to the renderer.
    EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
        selected_element, ASCIIToUTF16(kBobUsername),
        ASCIIToUTF16(kCarolPassword)));
    CheckTextFieldsDOMState(kBobUsername, true, kCarolPassword, true);
    username_length = strlen(kBobUsername);
    CheckUsernameSelection(username_length, username_length);

    ClearUsernameAndPasswordFields();
  }
}

// Tests that |FillSuggestion| properly fills the username and password when the
// username field is created dynamically in JavaScript.
TEST_F(PasswordAutofillAgentTest, FillSuggestionWithDynamicUsernameField) {
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  UpdateOnlyPasswordElement();

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  constexpr const char* kAddUsernameToFormScript =
      "var new_input = document.createElement('input');"
      "new_input.setAttribute('type', 'text');"
      "new_input.setAttribute('id', 'username');"
      "password_field = document.getElementById('password');"
      "password_field.parentNode.insertBefore(new_input, password_field);";
  ExecuteJavaScriptForTests(kAddUsernameToFormScript);
  UpdateUsernameAndPasswordElements();
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // After filling with the suggestion, both fields should be autocompleted.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      password_element_, ASCIIToUTF16(kAliceUsername),
      ASCIIToUTF16(kAlicePassword)));

  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  CheckFirstFillingResult(FillingResult::kWaitForUsername);
}

// Tests that |FillSuggestion| doesn't change non-empty non-autofilled username
// when interacting with the password field.
TEST_F(PasswordAutofillAgentTest,
       FillSuggestionFromPasswordFieldWithUsernameManuallyFilled) {
  username_element_.SetValue(WebString::FromUTF8("user1"));

  // Simulate the browser sending the login info, but set |wait_for_username| to
  // prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState("user1", false, std::string(), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, fake_driver_.called_show_manual_fallback_for_saving_count());

  // Only password field should be autocompleted.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      password_element_, ASCIIToUTF16(kAliceUsername),
      ASCIIToUTF16(kAlicePassword)));
  CheckTextFieldsDOMState("user1", false, kAlicePassword, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_driver_.called_show_manual_fallback_for_saving_count());

  // Try Filling with a different password. Only password should be changed.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      password_element_, ASCIIToUTF16(kBobUsername),
      ASCIIToUTF16(kCarolPassword)));
  CheckTextFieldsDOMState("user1", false, kCarolPassword, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_driver_.called_show_manual_fallback_for_saving_count());
}

// Tests that |FillSuggestion| properly fills the password if the username field
// is read-only.
TEST_F(PasswordAutofillAgentTest, FillSuggestionIfUsernameReadOnly) {
  // Simulate the browser sending the login info.
  SetElementReadOnly(username_element_, true);
  SimulateOnFillPasswordForm(fill_data_);

  for (const auto& selected_element : {username_element_, password_element_}) {
    // Neither field should be autocompleted.
    CheckTextFieldsDOMState(std::string(), false, std::string(), false);

    // Username field is not autocompletable, it should not be affected.
    EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
        selected_element, ASCIIToUTF16(kAliceUsername),
        ASCIIToUTF16(kAlicePassword)));
    CheckTextFieldsDOMState(std::string(), false, kAlicePassword, true);

    // Try Filling with a suggestion with password different from the one that
    // was initially sent to the renderer.
    EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
        selected_element, ASCIIToUTF16(kBobUsername),
        ASCIIToUTF16(kCarolPassword)));
    CheckTextFieldsDOMState(std::string(), false, kCarolPassword, true);

    ClearUsernameAndPasswordFields();
  }
}

// Tests that |PreviewSuggestion| properly previews the username and password.
TEST_F(PasswordAutofillAgentTest, PreviewSuggestion) {
  // Simulate the browser sending the login info, but set |wait_for_username| to
  // prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  for (const auto& selected_element : {username_element_, password_element_}) {
    // Neither field should be autocompleted.
    CheckTextFieldsDOMState(std::string(), false, std::string(), false);

    // If the password field is not autocompletable, it should not be affected.
    SetElementReadOnly(password_element_, true);
    EXPECT_FALSE(password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername, kAlicePassword));
    CheckTextFieldsSuggestedState(std::string(), false, std::string(), false);
    SetElementReadOnly(password_element_, false);

    // After selecting the suggestion, both fields should be previewed with
    // suggested values.
    EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername, kAlicePassword));
    CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
    // Since the suggestion is previewed as a placeholder, there should be no
    // selected text.
    CheckUsernameSelection(0, 0);

    // Try previewing with a password different from the one that was initially
    // sent to the renderer.
    EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
        selected_element, kBobUsername, kCarolPassword));
    CheckTextFieldsSuggestedState(kBobUsername, true, kCarolPassword, true);
    // Since the suggestion is previewed as a placeholder, there should be no
    // selected text.
    CheckUsernameSelection(0, 0);

    ClearUsernameAndPasswordFields();
  }
}

// Tests that |PreviewSuggestion| doesn't change non-empty non-autofilled
// username when previewing autofills on interacting with the password field.
TEST_F(PasswordAutofillAgentTest,
       PreviewSuggestionFromPasswordFieldWithUsernameManuallyFilled) {
  username_element_.SetValue(WebString::FromUTF8("user1"));

  // Simulate the browser sending the login info, but set |wait_for_username| to
  // prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState("user1", false, std::string(), false);

  // Only password field should be autocompleted.
  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      password_element_, kAliceUsername, kAlicePassword));
  CheckTextFieldsSuggestedState(std::string(), false, kAlicePassword, true);
  CheckTextFieldsDOMState("user1", false, std::string(), true);

  // Try previewing with a different password. Only password should be changed.
  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      password_element_, kBobUsername, kCarolPassword));
  CheckTextFieldsSuggestedState(std::string(), false, kCarolPassword, true);
  CheckTextFieldsDOMState("user1", false, std::string(), true);
}

// Tests that |PreviewSuggestion| properly previews the password if username is
// read-only.
TEST_F(PasswordAutofillAgentTest, PreviewSuggestionIfUsernameReadOnly) {
  // Simulate the browser sending the login info.
  SetElementReadOnly(username_element_, true);
  SimulateOnFillPasswordForm(fill_data_);

  for (const auto& selected_element : {username_element_, password_element_}) {
    // Neither field should be autocompleted.
    CheckTextFieldsDOMState(std::string(), false, std::string(), false);

    // Username field is not autocompletable, it should not be affected.
    EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername, kAlicePassword));
    // Password field must be autofilled.
    CheckTextFieldsSuggestedState(std::string(), false, kAlicePassword, true);

    // Try previewing with a password different from the one that was initially
    // sent to the renderer.
    EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
        selected_element, kBobUsername, kCarolPassword));
    CheckTextFieldsSuggestedState(std::string(), false, kCarolPassword, true);

    ClearUsernameAndPasswordFields();
  }
}

// Tests that |PreviewSuggestion| properly sets the username selection range.
TEST_F(PasswordAutofillAgentTest, PreviewSuggestionSelectionRange) {
  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  for (const auto& selected_element : {username_element_, password_element_}) {
    ResetFieldState(&username_element_, "ali", WebAutofillState::kPreviewed);
    ResetFieldState(&password_element_);

    EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername, kAlicePassword));
    CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
    // The selection should be set after the third character.
    CheckUsernameSelection(3, 3);
  }
}

// Tests that |ClearPreview| properly clears previewed username and password
// with password being previously autofilled.
TEST_F(PasswordAutofillAgentTest, ClearPreviewWithPasswordAutofilled) {
  ResetFieldState(&password_element_, "sec", WebAutofillState::kPreviewed);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState(std::string(), false, "sec", true);

  for (const auto& selected_element : {username_element_, password_element_}) {
    EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername, kAlicePassword));
    EXPECT_TRUE(
        password_autofill_agent_->DidClearAutofillSelection(selected_element));

    EXPECT_TRUE(username_element_.SuggestedValue().IsEmpty());
    EXPECT_TRUE(password_element_.SuggestedValue().IsEmpty());
    CheckTextFieldsDOMState(std::string(), false, "sec", true);
    CheckUsernameSelection(0, 0);
  }
}

// Tests that |ClearPreview| properly clears previewed username and password
// with username being previously autofilled.
TEST_F(PasswordAutofillAgentTest, ClearPreviewWithUsernameAutofilled) {
  ResetFieldState(&username_element_, "ali", WebAutofillState::kPreviewed);
  username_element_.SetSelectionRange(3, 3);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("ali", true, std::string(), false);

  for (const auto& selected_element : {username_element_, password_element_}) {
    EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername, kAlicePassword));
    EXPECT_TRUE(
        password_autofill_agent_->DidClearAutofillSelection(selected_element));

    EXPECT_TRUE(username_element_.SuggestedValue().IsEmpty());
    EXPECT_TRUE(password_element_.SuggestedValue().IsEmpty());
    CheckTextFieldsDOMState("ali", true, std::string(), false);
    CheckUsernameSelection(3, 3);
  }
}

// Tests that |ClearPreview| properly clears previewed username and password
// with username and password being previously autofilled.
TEST_F(PasswordAutofillAgentTest,
       ClearPreviewWithAutofilledUsernameAndPassword) {
  ResetFieldState(&username_element_, "ali", WebAutofillState::kPreviewed);
  ResetFieldState(&password_element_, "sec", WebAutofillState::kPreviewed);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("ali", true, "sec", true);

  for (const auto& selected_element : {username_element_, password_element_}) {
    EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername, kAlicePassword));
    EXPECT_TRUE(
        password_autofill_agent_->DidClearAutofillSelection(selected_element));

    EXPECT_TRUE(username_element_.SuggestedValue().IsEmpty());
    EXPECT_TRUE(password_element_.SuggestedValue().IsEmpty());
    CheckTextFieldsDOMState("ali", true, "sec", true);
    CheckUsernameSelection(3, 3);
  }
}

// Tests that TryToShowTouchToFill() works correctly for fillable and
// non-fillable fields.
TEST_F(PasswordAutofillAgentTest, TryToShowTouchToFillUsername) {
  // Initially no fill data is available.
  WebInputElement random_element = GetInputElementByID("random_field");
  EXPECT_FALSE(
      password_autofill_agent_->TryToShowTouchToFill(username_element_));
  EXPECT_FALSE(
      password_autofill_agent_->TryToShowTouchToFill(password_element_));
  EXPECT_FALSE(password_autofill_agent_->TryToShowTouchToFill(random_element));
  EXPECT_FALSE(password_autofill_agent_->ShouldSuppressKeyboard());

  // This changes once fill data is simulated. |random_element| continue  to
  // have no fill data, though.
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(
      password_autofill_agent_->TryToShowTouchToFill(username_element_));
  EXPECT_TRUE(password_autofill_agent_->ShouldSuppressKeyboard());
  EXPECT_EQ(WebAutofillState::kPreviewed, username_element_.GetAutofillState());
  EXPECT_EQ(WebAutofillState::kPreviewed, password_element_.GetAutofillState());

  EXPECT_CALL(fake_driver_, ShowTouchToFill);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAutofillAgentTest, TryToShowTouchToFillPassword) {
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(
      password_autofill_agent_->TryToShowTouchToFill(password_element_));
  EXPECT_TRUE(password_autofill_agent_->ShouldSuppressKeyboard());
  EXPECT_EQ(WebAutofillState::kPreviewed, password_element_.GetAutofillState());

  EXPECT_CALL(fake_driver_, ShowTouchToFill);
  base::RunLoop().RunUntilIdle();
}

#if defined(OS_ANDROID)
TEST_F(PasswordAutofillAgentTest, TouchToFillSuppressesPopups) {
  SimulateOnFillPasswordForm(fill_data_);
  SimulateSuggestionChoice(username_element_);
  EXPECT_CALL(fake_driver_, ShowTouchToFill);
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions).Times(0);
  base::RunLoop().RunUntilIdle();
}
#endif

TEST_F(PasswordAutofillAgentTest, DontTryToShowTouchToFillReadonlyPassword) {
  SetElementReadOnly(password_element_, true);
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_FALSE(
      password_autofill_agent_->TryToShowTouchToFill(password_element_));
}

TEST_F(PasswordAutofillAgentTest, DontShowTouchToFillOnSecurePageIfParamIsSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillTouchToFill, {{"insecure-origins-only", "true"}});

  // Reload the page with a secure origin.
  LoadHTMLWithUrlOverride(kFormHTML, "https://example.com");
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_FALSE(
      password_autofill_agent_->TryToShowTouchToFill(password_element_));
}

// Credentials are sent to the renderer even for sign-up forms as these may be
// eligible for filling via manual fall back. In this case, the username_field
// and password_field are not set. This test verifies that no failures are
// recorded in PasswordManager.FirstRendererFillingResult.
TEST_F(PasswordAutofillAgentTest, DontTryToShowTouchToFillSignUpForm) {
  LoadHTML(kSignupFormHTML);

  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element =
      document.GetElementById(WebString::FromUTF8("random_info"));
  ASSERT_FALSE(element.IsNull());
  username_element_ = element.To<WebInputElement>();

  fill_data_.username_field.unique_renderer_id = autofill::FieldRendererId();
  fill_data_.password_field.unique_renderer_id = autofill::FieldRendererId();

  WebFormElement form_element =
      document.GetElementById("LoginTestForm").To<WebFormElement>();
  fill_data_.form_renderer_id =
      autofill::FormRendererId(form_element.UniqueRendererFormId());

  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_FALSE(
      password_autofill_agent_->TryToShowTouchToFill(password_element_));
}

TEST_F(PasswordAutofillAgentTest, TouchToFillClosed) {
  SimulateOnFillPasswordForm(fill_data_);

  auto previous_state = password_element_.GetAutofillState();
  // Touch to fill will be shown multiple times until TouchToFillClosed()
  // gets called.
  EXPECT_TRUE(
      password_autofill_agent_->TryToShowTouchToFill(password_element_));
  EXPECT_TRUE(password_autofill_agent_->ShouldSuppressKeyboard());
  EXPECT_EQ(WebAutofillState::kPreviewed, password_element_.GetAutofillState());

  EXPECT_CALL(fake_driver_, ShowTouchToFill);
  base::RunLoop().RunUntilIdle();

  // Make sure that resetting Touch To Fill resets the Autofill state.
  password_autofill_agent_->TouchToFillClosed(true);
  EXPECT_FALSE(
      password_autofill_agent_->TryToShowTouchToFill(password_element_));
  EXPECT_FALSE(password_autofill_agent_->ShouldSuppressKeyboard());
  EXPECT_EQ(previous_state, password_element_.GetAutofillState());

  // Reload the page and simulate fill.
  LoadHTML(kFormHTML);
  UpdateOriginForHTML(kFormHTML);
  UpdateUsernameAndPasswordElements();
  SimulateOnFillPasswordForm(fill_data_);

  // After the reload touch to fill is shown again.
  EXPECT_TRUE(
      password_autofill_agent_->TryToShowTouchToFill(password_element_));
  EXPECT_TRUE(password_autofill_agent_->ShouldSuppressKeyboard());
  EXPECT_EQ(WebAutofillState::kPreviewed, password_element_.GetAutofillState());

  EXPECT_CALL(fake_driver_, ShowTouchToFill);
  base::RunLoop().RunUntilIdle();
}

// Tests that |FillIntoFocusedField| doesn't fill read-only text fields.
TEST_F(PasswordAutofillAgentTest, FillIntoFocusedReadonlyTextField) {
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // If the field is readonly, it should not be affected.
  SetElementReadOnly(username_element_, true);
  SimulateElementClick(kUsernameName);
  password_autofill_agent_->FillIntoFocusedField(
      /*is_password=*/false, ASCIIToUTF16(kAliceUsername));
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);
}

// Tests that |FillIntoFocusedField| properly fills user-provided credentials.
TEST_F(PasswordAutofillAgentTest, FillIntoFocusedWritableTextField) {
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // The same field should be filled if it is writable.
  FocusElement(kUsernameName);
  SetElementReadOnly(username_element_, false);

  password_autofill_agent_->FillIntoFocusedField(
      /*is_password=*/false, ASCIIToUTF16(kAliceUsername));
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), false);
  CheckUsernameSelection(strlen(kAliceUsername), strlen(kAliceUsername));
}

// Tests that |FillIntoFocusedField| doesn't fill passwords in userfields.
TEST_F(PasswordAutofillAgentTest, FillIntoFocusedFieldOnlyIntoPasswordFields) {
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // Filling a password into a username field doesn't work.
  SimulateElementClick(kUsernameName);

  password_autofill_agent_->FillIntoFocusedField(
      /*is_password=*/true, ASCIIToUTF16(kAlicePassword));
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // When a password field is focus, the filling works.
  SimulateElementClick(kPasswordName);

  password_autofill_agent_->FillIntoFocusedField(
      /*is_password=*/true, ASCIIToUTF16(kAlicePassword));
  CheckTextFieldsDOMState(std::string(), false, kAlicePassword, true);
}

// Tests that |FillIntoFocusedField| fills last focused, not last clicked field.
TEST_F(PasswordAutofillAgentTest, FillIntoFocusedFieldForNonClickFocus) {
  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // Click the username but shift the focus without click to the password.
  SimulateElementClick(kUsernameName);
  FocusElement(kPasswordName);
  // The completion should now affect ONLY the password field. Don't fill a
  // password so the error on failure shows where the filling happened.
  // (see FillIntoFocusedFieldOnlyIntoPasswordFields).

  password_autofill_agent_->FillIntoFocusedField(
      /*is_password=*/false, ASCIIToUTF16("TextToFill"));
  CheckTextFieldsDOMState(std::string(), false, "TextToFill", true);
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

  for (const auto& selected_element : {username_element_, password_element_}) {
    EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
        selected_element, kAliceUsername, kAlicePassword));
    EXPECT_TRUE(
        password_autofill_agent_->DidClearAutofillSelection(selected_element));

    EXPECT_TRUE(username_element_.SuggestedValue().IsEmpty());
    EXPECT_TRUE(password_element_.SuggestedValue().IsEmpty());
    CheckTextFieldsDOMState(std::string(), false, std::string(), false);
    CheckUsernameSelection(0, 0);
  }
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
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();

  SimulateSuggestionChoice(username_element_);
  CheckSuggestions(kAliceUsername, true);

  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// With butter, passwords fields should always trigger the popup so the user can
// unlock account-stored suggestions from there.
TEST_F(PasswordAutofillAgentTest, ShowPopupOnPasswordFieldWithoutSuggestions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);
  ClearUsernameAndPasswordFields();
  UpdateRendererIDs();

  SimulateElementClick(kPasswordName);

  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();
}

// Before butter, passwords fields should never trigger the popup on password
// passwords fields without suggestions since it would not be helpful.
TEST_F(PasswordAutofillAgentTest, NoPopupOnPasswordFieldWithoutSuggestions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);
  ClearUsernameAndPasswordFields();
  UpdateRendererIDs();

  SimulateElementClick(kPasswordName);

  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions).Times(0);
  base::RunLoop().RunUntilIdle();
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
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();

  // Simulate a user clicking on the username element. This should produce a
  // message with all the usernames.
  autofill_agent_->FormControlElementClicked(username_element_, false);
  CheckSuggestions(std::string(), false);

  // Now simulate a user typing in an unrecognized username and then
  // clicking on the username element. This should also produce a message with
  // all the usernames.
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions).Times(AtMost(3));
  SimulateUsernameTyping("baz");

  autofill_agent_->FormControlElementClicked(username_element_, true);
  CheckSuggestions("baz", true);
  ClearUsernameAndPasswordFields();
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
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();

  // Simulate a user clicking on the password element. This should produce no
  // message.
  autofill_agent_->FormControlElementClicked(password_element_, false);
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();
}

// The user types in a username and a password, but then just before sending
// the form off, a script clears them. This test checks that
// PasswordAutofillAgent can still remember the username and the password
// typed by the user.
TEST_F(PasswordAutofillAgentTest,
       DISABLED_RememberLastNonEmptyUsernameAndPasswordOnSubmit_ScriptCleared) {
  LoadHTML(kSignupFormHTML);
  WebInputElement username_element = GetInputElementByID("random_info");
  ASSERT_FALSE(username_element.IsNull());
  SimulateUserInputChangeForElement(&username_element, "username");

  WebInputElement new_password_element = GetInputElementByID("new_password");
  ASSERT_FALSE(new_password_element.IsNull());
  SimulateUserInputChangeForElement(&new_password_element, "random");

  WebInputElement confirmation_password_element =
      GetInputElementByID("confirm_password");
  ASSERT_FALSE(confirmation_password_element.IsNull());
  SimulateUserInputChangeForElement(&confirmation_password_element, "random");

  // Simulate that the username and the password values were cleared by the
  // site's JavaScript before submit.
  username_element.SetValue(WebString());
  new_password_element.SetValue(WebString());
  confirmation_password_element.SetValue(WebString());

  // Submit form.
  FormTracker* tracker = autofill_agent_->form_tracker_for_testing();
  static_cast<content::RenderFrameObserver*>(tracker)->WillSubmitForm(
      username_element.Form());

  // Observe that the PasswordAutofillAgent still remembered the last non-empty
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(GetFormUniqueRendererId("form"),
                                              "username", "", "random");
  fake_driver_.form_data_submitted();
}

// Similar to RememberLastNonEmptyPasswordOnSubmit_ScriptCleared, but this time
// it's the user who clears the username and the password. This test checks
// that in that case, the last non-empty username and password are not
// remembered.
TEST_F(PasswordAutofillAgentTest,
       RememberLastNonEmptyUsernameAndPasswordOnSubmit_UserCleared) {
  SimulateUsernameTyping("temp");
  SimulatePasswordTyping("random");

  // Simulate that the user actually cleared the username and password again.
  SimulateUsernameTyping("");
  SimulatePasswordTyping("");

  SubmitForm();

  // Observe that the PasswordAutofillAgent respects the user having cleared the
  // password.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), "", "", "");
}

// Similar to RememberLastNonEmptyPasswordOnSubmit_ScriptCleared, but uses the
// new password instead of the current password.
TEST_F(PasswordAutofillAgentTest,
       DISABLED_RememberLastNonEmptyUsernameAndPasswordOnSubmit_New) {
  const char kNewPasswordFormHTML[] =
      "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
      "  <INPUT type='text' id='username' autocomplete='username'/>"
      "  <INPUT type='password' id='password' autocomplete='new-password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kNewPasswordFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("temp");
  SimulatePasswordTyping("random");

  // Simulate that the username and the password value was cleared by
  // the site's JavaScript before submit.
  username_element_.SetValue(WebString());
  password_element_.SetValue(WebString());

  SubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the last non-empty
  // password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), "temp", "", "random");
}

// The user first accepts a suggestion, but then overwrites the password. This
// test checks that the overwritten password is not reverted back.
TEST_F(PasswordAutofillAgentTest,
       NoopEditingDoesNotOverwriteManuallyEditedPassword) {
  fill_data_.wait_for_username = true;
  SimulateUsernameTyping(kAliceUsername);
  SimulateOnFillPasswordForm(fill_data_);
  SimulateSuggestionChoice(username_element_);
  const std::string old_username(username_element_.Value().Utf8());
  const std::string old_password(password_element_.Value().Utf8());
  const std::string new_password(old_password + "modify");

  // The user changes the password.
  SimulatePasswordTyping(new_password);

  // Change focus in between to make sure blur events don't trigger filling.
  SetFocused(password_element_);
  SetFocused(username_element_);

  // The password should have stayed as the user changed it.
  // The username should not be autofilled, because it was typed by the user.
  CheckTextFieldsDOMState(old_username, false, new_password, false);
  // The password should not have a suggested value.
  CheckUsernameDOMStatePasswordSuggestedState(old_username, false,
                                              std::string(), false);
}

// The user types the username, then accepts a suggestion. This test checks
// that autofilling does not rewrite the username, if the value is already
// there.
TEST_F(PasswordAutofillAgentTest, AcceptingSuggestionDoesntRewriteUsername) {
  fill_data_.wait_for_username = true;
  SimulateUsernameTyping(kAliceUsername);
  SimulateOnFillPasswordForm(fill_data_);
  SimulateSuggestionChoice(username_element_);

  const std::string username(username_element_.Value().Utf8());
  const std::string password(password_element_.Value().Utf8());

  // The password was autofilled. The username was not.
  CheckTextFieldsDOMState(username, false, password, true);
}

// The user types in a username and a password, but then just before sending
// the form off, a script changes them. This test checks that
// PasswordAutofillAgent can still remember the username and the password
// typed by the user.
TEST_F(PasswordAutofillAgentTest,
       RememberLastTypedUsernameAndPasswordOnSubmit_ScriptChanged) {
  SimulateUsernameTyping("temp");
  SimulatePasswordTyping("random");

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.SetValue(WebString("new username"));
  password_element_.SetValue(WebString("new password"));

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), "temp", "random", "");
}

TEST_F(PasswordAutofillAgentTest, RememberFieldPropertiesOnSubmit) {
  SimulateUsernameTyping("temp");
  SimulatePasswordTyping("random");

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.SetValue(WebString("new username"));
  password_element_.SetValue(WebString("new password"));

  SaveAndSubmitForm();

  std::map<base::string16, FieldPropertiesMask> expected_properties_masks;
  expected_properties_masks[ASCIIToUTF16("random_field")] =
      FieldPropertiesFlags::kHadFocus;
  expected_properties_masks[ASCIIToUTF16("username")] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;
  expected_properties_masks[ASCIIToUTF16("password")] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;

  ExpectFieldPropertiesMasks(PasswordFormSubmitted, expected_properties_masks,
                             SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
}

TEST_F(PasswordAutofillAgentTest, FixEmptyFieldPropertiesOnSubmit) {
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate a user click so that the password field's real value is filled.
  SimulateElementClick(kUsernameName);

  // Simulate replacing the username and password field.
  static constexpr char kJavaScript[] =
      "const old_username = document.getElementById('username');"
      "const old_password = document.getElementById('password');"

      "const new_username = document.createElement('input');"
      "new_username.value = old_username.value;"
      "new_username.id = 'new_username';"

      "const new_password = document.createElement('input');"
      "new_password.value = old_password.value;"
      "new_password.id = 'new_password';"

      "const form = document.getElementById('LoginTestForm');"
      "form.appendChild(new_username);"
      "form.appendChild(new_password);"
      "form.removeChild(old_username);"
      "form.removeChild(old_password);";

  ExecuteJavaScriptForTests(kJavaScript);
  auto form_element = GetMainFrame()
                          ->GetDocument()
                          .GetElementById(WebString::FromUTF8("LoginTestForm"))
                          .To<WebFormElement>();
  SaveAndSubmitForm(form_element);

  std::map<base::string16, FieldPropertiesMask> expected_properties_masks;
  expected_properties_masks[ASCIIToUTF16("new_username")] =
      FieldPropertiesFlags::kAutofilledOnPageLoad;
  expected_properties_masks[ASCIIToUTF16("new_password")] =
      FieldPropertiesFlags::kAutofilledOnPageLoad;

  ExpectFieldPropertiesMasks(PasswordFormSubmitted, expected_properties_masks,
                             SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
}

TEST_F(PasswordAutofillAgentTest,
       RememberFieldPropertiesOnSameDocumentNavigation) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  std::string hide_elements =
      "var password = document.getElementById('password');"
      "password.style = 'display:none';"
      "var username = document.getElementById('username');"
      "username.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());

  FireAjaxSucceeded();

  std::map<base::string16, FieldPropertiesMask> expected_properties_masks;
  expected_properties_masks[ASCIIToUTF16("username")] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;
  expected_properties_masks[ASCIIToUTF16("password")] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;

  ExpectFieldPropertiesMasks(PasswordFormSameDocumentNavigation,
                             expected_properties_masks,
                             SubmissionIndicatorEvent::XHR_SUCCEEDED);
}

TEST_F(PasswordAutofillAgentTest,
       RememberFieldPropertiesOnSameDocumentNavigation_2) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  std::string hide_elements =
      "var password = document.getElementById('password');"
      "password.style = 'display:none';"
      "var username = document.getElementById('username');"
      "username.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());

  base::RunLoop().RunUntilIdle();

  std::map<base::string16, FieldPropertiesMask> expected_properties_masks;
  expected_properties_masks[ASCIIToUTF16("username")] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;
  expected_properties_masks[ASCIIToUTF16("password")] =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;

  ExpectFieldPropertiesMasks(PasswordFormSameDocumentNavigation,
                             expected_properties_masks,
                             SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR);
}

// The username/password is autofilled by password manager then just before
// sending the form off, a script changes them. This test checks that
// PasswordAutofillAgent can still get the username and the password autofilled.
TEST_F(PasswordAutofillAgentTest,
       RememberLastAutofilledUsernameAndPasswordOnSubmit_ScriptChanged) {
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.SetValue(WebString("new username"));
  password_element_.SetValue(WebString("new password"));

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the autofilled
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), kAliceUsername, kAlicePassword,
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

  SimulateUsernameTyping("temp");
  SimulatePasswordTyping("random");

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.SetValue(WebString("new username"));
  password_element_.SetValue(WebString("new password"));

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), "temp", "random", "");
}

// The user starts typing username then it is autofilled.
// PasswordAutofillAgent should remember the username that was autofilled,
// not last typed.
TEST_F(PasswordAutofillAgentTest, RememberAutofilledUsername) {
  SimulateUsernameTyping("Te");
  // Simulate that the username was changed by autofilling.
  username_element_.SetValue(WebString("temp"));
  SimulatePasswordTyping("random");

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), "temp", "random", "");
}

// The user starts typing username then javascript suggests to select another
// username that was generated based on typed field value (e.g. surname field).
// PasswordAutofillAgent should remember the username that was selected,
// not last typed.
TEST_F(PasswordAutofillAgentTest,
       RememberUsernameGeneratedBasingOnTypedFields) {
  SimulateUsernameTyping("Temp");
  SimulatePasswordTyping("random");

  // Suppose that "random_field" contains surname.
  WebInputElement surname_element = GetInputElementByID("random_field");
  SimulateUserInputChangeForElement(&surname_element, "Smith");

  // Simulate that the user selected username that was generated by script.
  username_element_.SetValue(WebString("foo.smith"));

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), "foo.smith", "random", "");
}

// If credentials contain username+password but the form contains only a
// password field, we don't autofill on page load.
TEST_F(PasswordAutofillAgentTest, DontFillFormWithNoUsername) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  UpdateOnlyPasswordElement();

  SimulateOnFillPasswordForm(fill_data_);

  // As the credential contains a username, but the form does not, the
  // credential is not filled.
  CheckFirstFillingResult(FillingResult::kFoundNoPasswordForUsername);
}

TEST_F(PasswordAutofillAgentTest, ShowPopupOnEmptyPasswordField) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  UpdateOnlyPasswordElement();
  fill_data_.username_field = FormFieldData();
  UpdateOriginForHTML(kVisibleFormWithNoUsernameHTML);
  fill_data_.additional_logins.clear();

  password_element_.SetValue("");
  password_element_.SetAutofillState(WebAutofillState::kNotFilled);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  // Show popup suggesstion when the password field is empty.
  password_element_.SetValue("");
  password_element_.SetAutofillState(WebAutofillState::kNotFilled);

  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, base::string16(), ASCIIToUTF16(kAlicePassword));
  CheckSuggestions(std::string(), true);
  EXPECT_EQ(ASCIIToUTF16(kAlicePassword), password_element_.Value().Utf16());
  EXPECT_TRUE(password_element_.IsAutofilled());
}

TEST_F(PasswordAutofillAgentTest, ShowPopupOnAutofilledPasswordField) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  UpdateOnlyPasswordElement();
  fill_data_.username_field = FormFieldData();
  UpdateOriginForHTML(kVisibleFormWithNoUsernameHTML);
  fill_data_.additional_logins.clear();

  password_element_.SetValue("");
  password_element_.SetAutofillState(WebAutofillState::kNotFilled);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  // Show popup suggesstion when the password field is autofilled.
  password_element_.SetValue("123");
  password_element_.SetAutofillState(WebAutofillState::kAutofilled);

  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, base::string16(), ASCIIToUTF16(kAlicePassword));
  CheckSuggestions(std::string(), true);
  EXPECT_EQ(ASCIIToUTF16(kAlicePassword), password_element_.Value().Utf16());
  EXPECT_TRUE(password_element_.IsAutofilled());
}

TEST_F(PasswordAutofillAgentTest, NotShowPopupPasswordField) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  UpdateOnlyPasswordElement();
  fill_data_.username_field = FormFieldData();
  UpdateOriginForHTML(kVisibleFormWithNoUsernameHTML);
  fill_data_.additional_logins.clear();

  password_element_.SetValue("");
  password_element_.SetAutofillState(WebAutofillState::kNotFilled);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  // Do not show popup suggesstion when the password field is not-empty and not
  // autofilled.
  password_element_.SetValue("123");
  password_element_.SetAutofillState(WebAutofillState::kNotFilled);

  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, base::string16(), ASCIIToUTF16(kAlicePassword));
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions).Times(0);
  base::RunLoop().RunUntilIdle();
}

// Tests with fill-on-account-select enabled that if the username element is
// read-only and filled with an unknown username, then the password field is not
// highlighted as autofillable (regression test for https://crbug.com/442564).
TEST_F(PasswordAutofillAgentTest,
       FillOnAccountSelectOnlyReadonlyUnknownUsername) {
  SetFillOnAccountSelect();

  ClearUsernameAndPasswordFields();

  username_element_.SetValue("foobar");
  SetElementReadOnly(username_element_, true);

  CheckUsernameDOMStatePasswordSuggestedState(std::string("foobar"), false,
                                              std::string(), false);
}

// The user types in a username and a password. Then JavaScript changes password
// field to readonly state before submit. PasswordAutofillAgent can correctly
// process readonly password field. This test models behaviour of gmail.com.
TEST_F(PasswordAutofillAgentTest, ReadonlyPasswordFieldOnSubmit) {
  SimulateUsernameTyping("temp");
  SimulatePasswordTyping("random");

  // Simulate that JavaScript makes password field readonly.
  SetElementReadOnly(password_element_, true);

  SubmitForm();

  // Observe that the PasswordAutofillAgent can correctly process submitted
  // form.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), "temp", "random", "");
}

// Verify that typed passwords are saved correctly when autofill and generation
// both trigger. Regression test for https://crbug.com/493455
TEST_F(PasswordAutofillAgentTest, PasswordGenerationTriggered_TypedPassword) {
  SimulateOnFillPasswordForm(fill_data_);

  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      "password" /* new_password_id */, nullptr /* confirm_password_id*/);

  // Generation event is triggered due to focus events.
  EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus())
      .Times(testing::AnyNumber());
  SimulateUsernameTyping("NewGuy");
  SimulatePasswordTyping("NewPassword");

  SaveAndSubmitForm();

  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), "NewGuy", "NewPassword", "");
}

// Verify that generated passwords are saved correctly when autofill and
// generation both trigger. Regression test for https://crbug.com/493455.
TEST_F(PasswordAutofillAgentTest,
       PasswordGenerationTriggered_GeneratedPassword) {
  SimulateOnFillPasswordForm(fill_data_);

  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      "password" /* new_password_id */, nullptr /* confirm_password_id*/);
  // Simulate the user clicks on a password field, that leads to showing
  // generaiton pop-up. GeneratedPasswordAccepted can't be called without it.
  SimulateElementClick(kPasswordName);

  base::string16 password = ASCIIToUTF16("NewPass22");
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
  password_generation_->GeneratedPasswordAccepted(password);

  SaveAndSubmitForm();

  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), kAliceUsername, "NewPass22",
      "");
}

TEST_F(PasswordAutofillAgentTest,
       ResetPasswordGenerationWhenFieldIsAutofilled) {
  // A user generates password.
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      "password" /* new_password_id */, nullptr /* confirm_password_id*/);
  // Simulate the user clicks on a password field, that leads to showing
  // generaiton pop-up. GeneratedPasswordAccepted can't be called without it.
  SimulateElementClick(kPasswordName);
  base::string16 password = ASCIIToUTF16("NewPass22");
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, Eq(password)));
  password_generation_->GeneratedPasswordAccepted(password);

  // The form should not be autofilled on the next call of FillPasswordForm
  EXPECT_CALL(fake_pw_client_, PasswordNoLongerGenerated(_));
  SimulateOnFillPasswordForm(fill_data_);
  base::RunLoop().RunUntilIdle();

  // The password field shoudln't reveal the value on focusing.
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("password"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement password_element = element.To<WebInputElement>();
  EXPECT_FALSE(password_element.ShouldRevealPassword());
  EXPECT_FALSE(password_element.IsAutofilled());

  SaveAndSubmitForm();

  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), kAliceUsername, "NewPass22",
      "");

  // Then user selects another account on Fill On Account Select
  SimulateSuggestionChoiceOfUsernameAndPassword(username_element_,
                                                ASCIIToUTF16(kBobUsername),
                                                ASCIIToUTF16(kBobPassword));
  base::RunLoop().RunUntilIdle();

  // The password field still shoudln't reveal the value on focusing.
  EXPECT_FALSE(password_element.ShouldRevealPassword());
  EXPECT_TRUE(password_element.IsAutofilled());

  SaveAndSubmitForm();

  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), kBobUsername, kBobPassword, "");
}

// If password generation is enabled for a field, password autofill should not
// show UI.
TEST_F(PasswordAutofillAgentTest, PasswordGenerationSupersedesAutofill) {
  LoadHTML(kSignupFormHTML);

  // Update password_element_;
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element =
      document.GetElementById(WebString::FromUTF8("new_password"));
  ASSERT_FALSE(element.IsNull());
  password_element_ = element.To<WebInputElement>();

  // Update fill_data_ for the new form and simulate filling. Pretend as if
  // the password manager didn't detect a username field so it will try to
  // show UI when the password field is focused.
  fill_data_.wait_for_username = true;
  fill_data_.username_field = FormFieldData();
  fill_data_.password_field.name = ASCIIToUTF16("new_password");
  UpdateOriginForHTML(kSignupFormHTML);
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate generation triggering.
  SetFoundFormEligibleForGeneration(
      password_generation_, GetMainFrame()->GetDocument(),
      "new_password" /* new_password_id */,
      "confirm_password" /* confirm_password_id*/);

  // Simulate the field being clicked to start typing. This should trigger
  // generation but not password autofill.
  SetFocused(password_element_);
  EXPECT_CALL(fake_pw_client_, AutomaticGenerationAvailable(_));
  SimulateElementClick("new_password");
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&fake_pw_client_);
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions).Times(0);
  base::RunLoop().RunUntilIdle();

  // On destruction the state is updated.
  EXPECT_CALL(fake_pw_client_, GenerationElementLostFocus())
      .Times(testing::AnyNumber());
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
  SimulateOnFillPasswordForm(fill_data_);

  for (const auto& selected_element : {username_element_, password_element_}) {
    // Neither field should be autocompleted.
    CheckTextFieldsDOMState(std::string(), false, std::string(), false);

    EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
        selected_element, ASCIIToUTF16(kAliceUsername),
        ASCIIToUTF16(kAlicePassword)));
    CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

    ClearUsernameAndPasswordFields();
  }
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
  SimulateOnFillPasswordForm(fill_data_);
  // Simulate a user clicking on the username element. This should produce a
  // message.
  autofill_agent_->FormControlElementClicked(username_element_, true);
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
  SimulateOnFillPasswordForm(fill_data_);
  // Simulate a user clicking on the password element. This should produce a
  // message.
  autofill_agent_->FormControlElementClicked(password_element_, true);
  CheckSuggestions("", true);
}

// Tests that only the password field is autocompleted when the browser sends
// back data with only one credentials and empty username.
TEST_F(PasswordAutofillAgentTest, NotAutofillNoUsername) {
  ClearField(&fill_data_.username_field);
  fill_data_.additional_logins.clear();
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState("", false, kAlicePassword, true);
}

// Tests that the username field is not marked as autofilled when fill data has
// the empty username.
TEST_F(PasswordAutofillAgentTest,
       AutofillNoUsernameWhenOtherCredentialsStored) {
  fill_data_.username_field.value.clear();
  ASSERT_FALSE(fill_data_.additional_logins.empty());
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState("", false, kAlicePassword, true);
}

TEST_F(PasswordAutofillAgentTest, NoForm_PromptForAJAXSubmitWithoutNavigation) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  std::string hide_elements =
      "var password = document.getElementById('password');"
      "password.style = 'display:none';"
      "var username = document.getElementById('username');"
      "username.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());

  FireAjaxSucceeded();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      autofill::FormRendererId(), "Bob", "mypassword", "",
      SubmissionIndicatorEvent::XHR_SUCCEEDED);
}

TEST_F(PasswordAutofillAgentTest,
       NoForm_PromptForAJAXSubmitWithoutNavigation_2) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  std::string hide_elements =
      "var password = document.getElementById('password');"
      "password.style = 'display:none';"
      "var username = document.getElementById('username');"
      "username.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());

  base::RunLoop().RunUntilIdle();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      autofill::FormRendererId(), "Bob", "mypassword", "",
      SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR);
}

// In this test, a <div> wrapping a form is hidden via display:none after an
// Ajax request. The test verifies that we offer to save the password, as hiding
// the <div> also hiding the <form>.
TEST_F(PasswordAutofillAgentTest, PromptForAJAXSubmitAfterHidingParentElement) {
  LoadHTML(kDivWrappedFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  std::string hide_element =
      "var outerDiv = document.getElementById('outer');"
      "outerDiv.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_element.c_str());

  base::RunLoop().RunUntilIdle();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      GetFormUniqueRendererId("form"), "Bob", "mypassword", "",
      SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR);
}

// In this test, a <div> wrapping a form is removed from the DOM after an Ajax
// request. The test verifies that we offer to save the password, as removing
// the <div> also removes the <form>.
TEST_F(PasswordAutofillAgentTest,
       PromptForAJAXSubmitAfterDeletingParentElement) {
  LoadHTML(kDivWrappedFormHTML);
  UpdateUsernameAndPasswordElements();
  autofill::FormRendererId renderer_id = GetFormUniqueRendererId("form");

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  std::string delete_element =
      "var outerDiv = document.getElementById('outer');"
      "var innerDiv = document.getElementById('inner');"
      "outerDiv.removeChild(innerDiv);";
  ExecuteJavaScriptForTests(delete_element.c_str());

  base::RunLoop().RunUntilIdle();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      renderer_id, "Bob", "mypassword", "",
      SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR);
}

TEST_F(PasswordAutofillAgentTest,
       NoForm_NoPromptForAJAXSubmitWithoutNavigationAndElementsVisible) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_driver_.called_password_form_submitted());
}

// Tests that no save promt is shown when an unowned form is changed and AJAX
// completed but the form is still visible.
TEST_F(PasswordAutofillAgentTest,
       NoForm_NoPromptForAJAXSubmitWithoutNavigationAndNewElementAppeared) {
  const char kNoFormHTMLWithHiddenField[] =
      "<INPUT type='text' id='username'/>"
      "<INPUT type='password' id='password'/>"
      "<INPUT type='text' id='captcha' style='display:none'/>";
  LoadHTML(kNoFormHTMLWithHiddenField);

  UpdateUsernameAndPasswordElements();
  WebElement captcha_element = GetMainFrame()->GetDocument().GetElementById(
      WebString::FromUTF8("captcha"));
  ASSERT_FALSE(captcha_element.IsNull());

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  // Simulate captcha element show up right before AJAX completed.
  std::string show_captcha =
      "var captcha = document.getElementById('captcha');"
      "captcha.style = 'display:inline';";
  ExecuteJavaScriptForTests(show_captcha.c_str());

  FireAjaxSucceeded();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_same_document_navigation());
  EXPECT_FALSE(fake_driver_.called_password_form_submitted());
}

TEST_F(PasswordAutofillAgentTest,
       NoForm_NoPromptForAJAXSubmitWithoutNavigationAndNewElementAppeared_2) {
  const char kNoFormHTMLWithHiddenField[] =
      "<INPUT type='text' id='username'/>"
      "<INPUT type='password' id='password'/>"
      "<INPUT type='text' id='captcha' style='display:none'/>";
  LoadHTML(kNoFormHTMLWithHiddenField);

  UpdateUsernameAndPasswordElements();
  WebElement captcha_element = GetMainFrame()->GetDocument().GetElementById(
      WebString::FromUTF8("captcha"));
  ASSERT_FALSE(captcha_element.IsNull());

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  // Simulate captcha element show up right after AJAX completed.
  std::string show_captcha =
      "var captcha = document.getElementById('captcha');"
      "captcha.style = 'display:inline';";
  ExecuteJavaScriptForTests(show_captcha.c_str());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_driver_.called_same_document_navigation());
  EXPECT_FALSE(fake_driver_.called_password_form_submitted());
}

// Tests that no save promt is shown when a form with empty action URL is
// changed and AJAX completed but the form is still visible.
TEST_F(PasswordAutofillAgentTest,
       NoAction_NoPromptForAJAXSubmitWithoutNavigationAndNewElementAppeared) {
  // Form without an action URL.
  const char kHTMLWithHiddenField[] =
      "<FORM name='LoginTestForm'>"
      "  <INPUT type='text' id='username'/>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='text' id='captcha' style='display:none'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  // Set the valid URL so the form action URL can be generated properly.
  LoadHTMLWithUrlOverride(kHTMLWithHiddenField, "https://www.example.com");

  UpdateUsernameAndPasswordElements();
  WebElement captcha_element = GetMainFrame()->GetDocument().GetElementById(
      WebString::FromUTF8("captcha"));
  ASSERT_FALSE(captcha_element.IsNull());

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  // Simulate captcha element show up right before AJAX completed.
  captcha_element.SetAttribute("style", "display:inline;");

  FireAjaxSucceeded();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_same_document_navigation());
  EXPECT_FALSE(fake_driver_.called_password_form_submitted());
}

TEST_F(PasswordAutofillAgentTest,
       NoAction_NoPromptForAJAXSubmitWithoutNavigationAndNewElementAppeared_2) {
  // Form without an action URL.
  const char kHTMLWithHiddenField[] =
      "<FORM name='LoginTestForm'>"
      "  <INPUT type='text' id='username'/>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='text' id='captcha' style='display:none'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  // Set the valid URL so the form action URL can be generated properly.
  LoadHTMLWithUrlOverride(kHTMLWithHiddenField, "https://www.example.com");

  UpdateUsernameAndPasswordElements();
  WebElement captcha_element = GetMainFrame()->GetDocument().GetElementById(
      WebString::FromUTF8("captcha"));
  ASSERT_FALSE(captcha_element.IsNull());

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  // Simulate captcha element show up right after AJAX completed.
  captcha_element.SetAttribute("style", "display:inline;");

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_driver_.called_same_document_navigation());
  EXPECT_FALSE(fake_driver_.called_password_form_submitted());
}

TEST_F(PasswordAutofillAgentTest, DriverIsInformedAboutUnfillableField) {
  EXPECT_EQ(FocusedFieldType::kUnknown, fake_driver_.last_focused_field_type());
  SimulateElementClick(kPasswordName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillablePasswordField,
            fake_driver_.last_focused_field_type());

  // Even though the focused element is a username field, it should be treated
  // as unfillable, since it is read-only.
  SetElementReadOnly(username_element_, true);
  FocusElement(kUsernameName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kUnfillableElement,
            fake_driver_.last_focused_field_type());
}

TEST_F(PasswordAutofillAgentTest, DriverIsInformedAboutFillableFields) {
  SimulateElementClick("random_field");
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableNonSearchField,
            fake_driver_.last_focused_field_type());

  // A username field without fill data is indistinguishable from any other text
  // field.
  SimulateElementClick(kUsernameName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableNonSearchField,
            fake_driver_.last_focused_field_type());

  SimulateElementClick(kPasswordName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillablePasswordField,
            fake_driver_.last_focused_field_type());

  // A username field with fill data should be detected.
  SimulateOnFillPasswordForm(fill_data_);
  SimulateElementClick(kUsernameName);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableUsernameField,
            fake_driver_.last_focused_field_type());
}

TEST_F(PasswordAutofillAgentTest, DriverIsInformedAboutFillablSearchField) {
  LoadHTML(kSearchFieldHTML);
  SimulateElementClick(kSearchField);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableSearchField,
            fake_driver_.last_focused_field_type());
}

TEST_F(PasswordAutofillAgentTest, DriverIsInformedAboutFillableTextArea) {
  LoadHTML(kSocialNetworkPostFormHTML);

  SimulateElementClick(kSocialMediaTextArea);
  fake_driver_.Flush();
  EXPECT_EQ(FocusedFieldType::kFillableTextArea,
            fake_driver_.last_focused_field_type());
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
      {kFormContainsEmptyNamesHTML, true, kDummyUsernameField,
       kDummyPasswordField, kAliceUsername, kAlicePassword, true, true},

      // Password form with ambiguous name or id attributes specified for the
      // input fields.
      {kFormContainsAmbiguousNamesHTML, true, "credentials", "credentials",
       kAliceUsername, kAlicePassword, true, true},

      // Change password form without name or id attributes specified for the
      // input fields and |autocomplete='current-password'| attribute for old
      // password field.
      {kChangePasswordFormContainsEmptyNamesHTML, true, kDummyUsernameField,
       kDummyPasswordField, kAliceUsername, kAlicePassword, true, true},

      // Change password form without username field.
      {kChangePasswordFormButNoUsername, true, kEmpty, kDummyPasswordField,
       kEmpty, kAlicePassword, false, true},

      // Change password form without name or id attributes specified for the
      // input fields and |autocomplete='new-password'| attribute for new
      // password fields. This form *do not* trigger |OnFillPasswordForm| from
      // browser.
      {kChangePasswordFormButNoOldPassword, false, kDummyUsernameField,
       kDummyPasswordField, kEmpty, kEmpty, false, false},

      // Change password form without name or id attributes specified for the
      // input fields but |autocomplete='current-password'| or
      // |autocomplete='new-password'| attributes are missing for old and new
      // password fields respectively.
      {kChangePasswordFormButNoAutocompleteAttribute, true, kDummyUsernameField,
       kDummyPasswordField, kAliceUsername, kAlicePassword, true, true},
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
    blink::WebDocument document = GetMainFrame()->GetDocument();
    blink::WebVector<WebFormElement> forms = document.Forms();
    WebFormElement form_element = forms[0];
    std::vector<blink::WebFormControlElement> control_elements =
        form_util::ExtractAutofillableElementsInForm(form_element);
    bool has_fillable_username =
        (kEmpty != test_case.fill_data_username_field_name);
    if (has_fillable_username) {
      username_element_ = control_elements[0].To<WebInputElement>();
      password_element_ = control_elements[1].To<WebInputElement>();
    } else {
      password_element_ = control_elements[0].To<WebInputElement>();
    }

    UpdateOriginForHTML(test_case.html_form);
    if (test_case.does_trigger_autocomplete_on_fill) {
      // Prepare |fill_data_| to trigger autocomplete.
      fill_data_.username_field.name =
          ASCIIToUTF16(test_case.fill_data_username_field_name);
      fill_data_.password_field.name =
          ASCIIToUTF16(test_case.fill_data_password_field_name);
      fill_data_.additional_logins.clear();

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

  SaveAndSubmitForm();

  // Observe that the PasswordAutofillAgent sends to the browser selected
  // credentials.
  ExpectFormSubmittedWithUsernameAndPasswords(
      GetFormUniqueRendererId("LoginTestForm"), kBobUsername, kBobPassword, "");
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
  CheckSuggestions(std::string(), true);
  SimulateElementClick("password2");
  CheckSuggestions(std::string(), true);
}

// Tests that password manager sees both autofill assisted and user entered
// data on saving that is triggered by AJAX succeeded.
TEST_F(PasswordAutofillAgentTest,
       UsernameChangedAfterPasswordInput_AJAXSucceeded) {
  for (auto change_source :
       {FieldChangeSource::USER, FieldChangeSource::AUTOFILL,
        FieldChangeSource::USER_AUTOFILL}) {
    LoadHTML(kNoFormHTML);
    UpdateUsernameAndPasswordElements();

    SimulateUsernameTyping("Bob");
    SimulatePasswordTyping("mypassword");
    SimulateUsernameFieldChange(change_source);

    // Hide form elements to simulate successful login.
    std::string hide_elements =
        "var password = document.getElementById('password');"
        "password.style = 'display:none';"
        "var username = document.getElementById('username');"
        "username.style = 'display:none';";
    ExecuteJavaScriptForTests(hide_elements.c_str());

    FireAjaxSucceeded();

    ExpectSameDocumentNavigationWithUsernameAndPasswords(
        autofill::FormRendererId(), "Alice", "mypassword", "",
        SubmissionIndicatorEvent::XHR_SUCCEEDED);
  }
}

TEST_F(PasswordAutofillAgentTest,
       UsernameChangedAfterPasswordInput_AJAXSucceeded_2) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");
  SimulateUsernameTyping("Alice");

  FireAjaxSucceeded();

  // Hide form elements to simulate successful login.
  std::string hide_elements =
      "var password = document.getElementById('password');"
      "password.style = 'display:none';"
      "var username = document.getElementById('username');"
      "username.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());
  base::RunLoop().RunUntilIdle();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      autofill::FormRendererId(), "Alice", "mypassword", "",
      SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR);
}

// Tests that password manager sees both autofill assisted and user entered
// data on saving that is triggered by form submission.
TEST_F(PasswordAutofillAgentTest,
       UsernameChangedAfterPasswordInput_FormSubmitted) {
  for (auto change_source :
       {FieldChangeSource::USER, FieldChangeSource::AUTOFILL,
        FieldChangeSource::USER_AUTOFILL}) {
    LoadHTML(kFormHTML);
    UpdateUsernameAndPasswordElements();
    SimulateUsernameTyping("Bob");
    SimulatePasswordTyping("mypassword");
    SimulateUsernameFieldChange(change_source);

    SaveAndSubmitForm();

    ExpectFormSubmittedWithUsernameAndPasswords(
        GetFormUniqueRendererId("LoginTestForm"), "Alice", "mypassword", "");
  }
}

// Tests that a suggestion dropdown is shown on a password field even if a
// username field is present.
TEST_F(PasswordAutofillAgentTest, SuggestPasswordFieldSignInForm) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();

  // Simulate a user clicking on the password element. This should produce a
  // dropdown with suggestion of all available usernames.
  autofill_agent_->FormControlElementClicked(password_element_, false);
  CheckSuggestions("", true);
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
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();

  // Simulate a user clicking on the password elements. This should produce
  // dropdowns with suggestion of all available usernames.
  SimulateElementClick("password");
  CheckSuggestions("", true);

  SimulateElementClick("newpassword");
  CheckSuggestions("", true);

  SimulateElementClick("confirmpassword");
  CheckSuggestions("", true);

  // The user chooses to autofill the current password field.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      password_element_, ASCIIToUTF16(kAliceUsername),
      ASCIIToUTF16(kAlicePassword)));

  // Simulate a user clicking on not autofilled password fields. This should
  // produce no suggestion dropdowns.
  SimulateElementClick("newpassword");
  SimulateElementClick("confirmpassword");
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions).Times(0);
  base::RunLoop().RunUntilIdle();

  // But when the user clicks on the autofilled password field again it should
  // still produce a suggestion dropdown.
  SimulateElementClick("password");
  CheckSuggestions("", true);
}

TEST_F(PasswordAutofillAgentTest, ShowAutofillSignaturesFlag) {
  // Tests that form signature is set iff the flag is enabled.
  const bool kFalseTrue[] = {false, true};
  for (bool show_signatures : kFalseTrue) {
    if (show_signatures)
      EnableShowAutofillSignatures();

    LoadHTML(kFormHTML);
    WebDocument document = GetMainFrame()->GetDocument();
    WebFormElement form_element =
        document.GetElementById(WebString::FromASCII("LoginTestForm"))
            .To<WebFormElement>();
    ASSERT_FALSE(form_element.IsNull());

    // Check only form signature attribute. The full test is in
    // "PasswordGenerationAgentTestForHtmlAnnotation.*".
    WebString form_signature_attribute = WebString::FromASCII("form_signature");
    EXPECT_EQ(form_element.HasAttribute(form_signature_attribute),
              show_signatures);
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
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();

  // Simulate a user clicking on the password element. This should produce a
  // dropdown with suggestion of all available usernames.
  autofill_agent_->FormControlElementClicked(password_element_, false);
  CheckSuggestions("", true);
}

// Checks that a same-document navigation form submission could have an empty
// username.
TEST_F(PasswordAutofillAgentTest,
       SameDocumentNavigationSubmissionUsernameIsEmpty) {
  username_element_.SetValue(WebString());
  SimulatePasswordTyping("random");
  autofill::FormRendererId renderer_id =
      GetFormUniqueRendererId("LoginTestForm");

  // Simulate that JavaScript removes the submitted form from DOM. That means
  // that a submission was successful.
  std::string remove_form =
      "var form = document.getElementById('LoginTestForm');"
      "form.parentNode.removeChild(form);";
  ExecuteJavaScriptForTests(remove_form.c_str());

  FireDidCommitProvisionalLoad();

  ExpectSameDocumentNavigationWithUsernameAndPasswords(
      renderer_id, std::string(), "random", std::string(),
      SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
}

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
// Verify CheckSafeBrowsingReputation() is called when user starts filling
// a password field, and that this function is only called once.
TEST_F(PasswordAutofillAgentTest,
       CheckSafeBrowsingReputationWhenUserStartsFillingUsernamePassword) {
  ASSERT_EQ(0, fake_driver_.called_check_safe_browsing_reputation_cnt());
  // Simulate a click on password field to set its on focus,
  // CheckSafeBrowsingReputation() should be called.
  SimulateElementClick(kPasswordName);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_driver_.called_check_safe_browsing_reputation_cnt());

  // Subsequent editing will not trigger CheckSafeBrowsingReputation.
  SimulatePasswordTyping("modify");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_driver_.called_check_safe_browsing_reputation_cnt());

  // No CheckSafeBrowsingReputation() call on username field click.
  SimulateElementClick(kUsernameName);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_driver_.called_check_safe_browsing_reputation_cnt());

  SimulateElementClick(kPasswordName);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_driver_.called_check_safe_browsing_reputation_cnt());

  // Navigate to another page and click on password field,
  // CheckSafeBrowsingReputation() should be triggered again.
  LoadHTML(kFormHTML);
  SimulateElementClick(kPasswordName);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_driver_.called_check_safe_browsing_reputation_cnt());
}
#endif

// Tests that username/password are autofilled when JavaScript is changing url
// between discovering a form and receving credentials from the browser process.
TEST_F(PasswordAutofillAgentTest, AutocompleteWhenPageUrlIsChanged) {
  // Simulate that JavaScript changes url.
  fill_data_.origin = GURL(fill_data_.origin.possibly_invalid_spec() + "/path");

  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
}

// Regression test for https://crbug.com/728028.
TEST_F(PasswordAutofillAgentTest, NoForm_MultipleAJAXEventsWithoutSubmission) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateUsernameTyping("Bob");
  SimulatePasswordTyping("mypassword");

  FireAjaxSucceeded();

  base::RunLoop().RunUntilIdle();

  // Repeatedly occurring AJAX events without removing the input elements
  // shouldn't be treated as a password submission.

  FireAjaxSucceeded();

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(fake_driver_.called_password_form_submitted());
  ASSERT_FALSE(static_cast<bool>(fake_driver_.form_data_submitted()));
}

TEST_F(PasswordAutofillAgentTest, ManualFallbackForSaving) {
  EXPECT_CALL(fake_pw_client_, PresaveGeneratedPassword(_, _)).Times(0);
  // The users enters a username. No password - no fallback.
  SimulateUsernameTyping(kUsernameName);
  EXPECT_EQ(0, fake_driver_.called_show_manual_fallback_for_saving_count());

  // The user enters a password.
  SimulatePasswordTyping(kPasswordName);
  // SimulateUsernameTyping/SimulatePasswordTyping calls
  // PasswordAutofillAgent::UpdateStateForTextChange only once.
  EXPECT_EQ(1, fake_driver_.called_show_manual_fallback_for_saving_count());

  // Remove one character from the password value.
  SimulateUserTypingASCIICharacter(ui::VKEY_BACK, true);
  EXPECT_EQ(2, fake_driver_.called_show_manual_fallback_for_saving_count());

  // Add one character to the username value.
  SetFocused(username_element_);
  SimulateUserTypingASCIICharacter('a', true);
  EXPECT_EQ(3, fake_driver_.called_show_manual_fallback_for_saving_count());

  // Remove username value.
  SimulateUsernameTyping("");
  EXPECT_EQ(4, fake_driver_.called_show_manual_fallback_for_saving_count());

  // Change the password. Despite of empty username the fallback is still
  // there.
  SetFocused(password_element_);
  SimulateUserTypingASCIICharacter('a', true);
  EXPECT_EQ(5, fake_driver_.called_show_manual_fallback_for_saving_count());

  // Remove password value. The fallback should be disabled.
  SimulatePasswordTyping("");
  EXPECT_EQ(0, fake_driver_.called_show_manual_fallback_for_saving_count());

  // The user enters new password. Show the fallback again.
  SimulateUserTypingASCIICharacter('a', true);
  EXPECT_EQ(1, fake_driver_.called_show_manual_fallback_for_saving_count());
}

TEST_F(PasswordAutofillAgentTest, ManualFallbackForSaving_PasswordChangeForm) {
  LoadHTML(kPasswordChangeFormHTML);
  UpdateOriginForHTML(kPasswordChangeFormHTML);
  UpdateUsernameAndPasswordElements();

  // No password to save yet - no fallback.
  SimulateUsernameTyping(kUsernameName);
  EXPECT_EQ(0, fake_driver_.called_show_manual_fallback_for_saving_count());

  // The user enters in the current password field. The fallback should be
  // available to save the entered value.
  SimulatePasswordTyping(kPasswordName);
  // SimulateUsernameTyping/SimulatePasswordTyping calls
  // PasswordAutofillAgent::UpdateStateForTextChange only once.
  EXPECT_EQ(1, fake_driver_.called_show_manual_fallback_for_saving_count());

  // The user types into the new password field. The fallback should be updated.
  WebInputElement new_password = GetInputElementByID("newpassword");
  ASSERT_FALSE(new_password.IsNull());
  SetFocused(new_password);
  SimulateUserTypingASCIICharacter('a', true);
  EXPECT_EQ(2, fake_driver_.called_show_manual_fallback_for_saving_count());

  // Edits of the confirmation password field trigger fallback updates.
  WebInputElement confirmation_password =
      GetInputElementByID("confirmpassword");
  ASSERT_FALSE(confirmation_password.IsNull());
  SetFocused(confirmation_password);
  SimulateUserTypingASCIICharacter('a', true);
  EXPECT_EQ(3, fake_driver_.called_show_manual_fallback_for_saving_count());

  // Clear all password fields. The fallback should be disabled.
  SimulatePasswordTyping("");
  SimulateUserInputChangeForElement(&new_password, "");
  SimulateUserInputChangeForElement(&confirmation_password, "");
  EXPECT_EQ(0, fake_driver_.called_show_manual_fallback_for_saving_count());
}

// Tests that information about Gaia reauthentication form is sent to the
// browser with information that the password should not be saved.
TEST_F(PasswordAutofillAgentTest, GaiaReauthenticationFormIgnored) {
  // HTML is already loaded in test SetUp method, so information about password
  // forms was already sent to the |fake_driver_|. Hence it should be reset.
  fake_driver_.reset_password_forms_calls();

  const char kGaiaReauthenticationFormHTML[] =
      "<FORM id='ReauthenticationForm'>"
      "  <INPUT type='hidden' name='continue' "
      "value='https://passwords.google.com/'>"
      "  <INPUT type='hidden' name='rart'>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";

  LoadHTMLWithUrlOverride(kGaiaReauthenticationFormHTML,
                          "https://accounts.google.com");
  UpdateOnlyPasswordElement();

  // Simulate a user clicking on the password element.
  autofill_agent_->FormControlElementClicked(password_element_, false);

  fake_driver_.Flush();
  // Check that information about Gaia reauthentication is sent to the browser.
  ASSERT_TRUE(fake_driver_.called_password_forms_parsed());
  const std::vector<autofill::FormData>& parsed_form_data =
      fake_driver_.form_data_parsed().value();
  ASSERT_EQ(1u, parsed_form_data.size());
  EXPECT_TRUE(parsed_form_data[0].is_gaia_with_skip_save_password_form);
}

TEST_F(PasswordAutofillAgentTest,
       UpdateSuggestionsIfNewerCredentialsAreSupplied) {
  // Supply old fill data
  password_autofill_agent_->FillPasswordForm(fill_data_);
  // The username and password should have been autocompleted.
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  // Change fill data
  fill_data_.password_field.value = ASCIIToUTF16("a-changed-password");
  // Supply changed fill data
  password_autofill_agent_->FillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, "a-changed-password",
                                true);
}

TEST_F(PasswordAutofillAgentTest, SuggestLatestCredentials) {
  password_autofill_agent_->FillPasswordForm(fill_data_);
  SimulateElementClick(kPasswordName);
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  base::RunLoop().RunUntilIdle();

  // Change fill data
  fill_data_.username_field.value = ASCIIToUTF16("a-changed-username");

  password_autofill_agent_->FillPasswordForm(fill_data_);
  SimulateElementClick(kPasswordName);
  // Empty value because nothing was typed into the field.
  CheckSuggestions("", true);
}

// Tests that PSL matched password is not autofilled even when there is
// a prefilled username.
TEST_F(PasswordAutofillAgentTest, PSLMatchedPasswordIsNotAutofill) {
  const char kFormWithPrefilledUsernameHTML[] =
      "<FORM id='LoginTestForm' action='http://www.bidule.com'>"
      "  <INPUT type='text' id='username' value='prefilledusername'/>"
      "  <INPUT type='password' id='password'/>"
      "</FORM>";
  LoadHTML(kFormWithPrefilledUsernameHTML);

  // Retrieve the input elements so the test can access them.
  UpdateUsernameAndPasswordElements();

  // Set the expected form origin and action URLs.
  UpdateOriginForHTML(kFormWithPrefilledUsernameHTML);

  // Add PSL matched credentials with username equal to prefilled one.
  PasswordAndMetadata psl_credentials;
  psl_credentials.password = ASCIIToUTF16("pslpassword");
  // Non-empty realm means PSL matched credentials.
  psl_credentials.realm = "example.com";
  psl_credentials.username = ASCIIToUTF16("prefilledusername");
  fill_data_.additional_logins.push_back(std::move(psl_credentials));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Test that PSL matched password is not autofilled.
  CheckUsernameDOMStatePasswordSuggestedState("prefilledusername", false, "",
                                              false);
}

// Tests that the password form is filled as expected on load.
TEST_F(PasswordAutofillAgentTest, FillOnLoadWith) {
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  CheckFirstFillingResult(FillingResult::kSuccess);
}

// Tests that the password form is filled as expected on load even if form/field
// attributes were changed between from load and filling.
TEST_F(PasswordAutofillAgentTest, FillOnLoadFormChanged) {
  // Simulate JavaScript changed field names and form name.
  fill_data_.name += ASCIIToUTF16("1");
  fill_data_.username_field.name += ASCIIToUTF16("1");
  fill_data_.password_field.name += ASCIIToUTF16("1");

  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
}

TEST_F(PasswordAutofillAgentTest, FillOnLoadNoForm) {
  LoadHTML(kNoFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
}

TEST_F(PasswordAutofillAgentTest, FillOnLoadNoUsername) {
  LoadHTML(kTwoNoUsernameFormsHTML);
  username_element_.Reset();
  fill_data_.username_field.value.clear();
  password_element_ = GetInputElementByID("password2");
  UpdateRendererIDs();
  SimulateOnFillPasswordForm(fill_data_);
  EXPECT_EQ(kAlicePassword, password_element_.SuggestedValue().Utf8());
}

TEST_F(PasswordAutofillAgentTest, MayUsePlaceholderNoPlaceholder) {
  fill_data_.username_may_use_prefilled_placeholder = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
}

TEST_F(PasswordAutofillAgentTest, MayUsePlaceholderAndPlaceholderOnForm) {
  username_element_.SetValue(WebString::FromUTF8("placeholder"));
  fill_data_.username_may_use_prefilled_placeholder = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);
}

TEST_F(PasswordAutofillAgentTest, NoMayUsePlaceholderAndPlaceholderOnForm) {
  username_element_.SetValue(WebString::FromUTF8("placeholder"));
  fill_data_.username_may_use_prefilled_placeholder = false;

  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("placeholder", false, "", false);
}

TEST_F(PasswordAutofillAgentTest, AutofillsAfterUserGesture) {
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  password_autofill_agent_->UserGestureObserved();
  // It's a way to call PasswordValueGatekeeper::Reset().
  password_autofill_agent_->ReadyToCommitNavigation(nullptr);

  fill_data_.username_may_use_prefilled_placeholder = true;
  fill_data_.password_field.value = ASCIIToUTF16(kBobPassword);

  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsStateForElements(
      username_element_, kAliceUsername,
      /* username_autofilled */ true, password_element_, kBobPassword,
      /* password_autofilled */ true, /* check_suggested_username */ false,
      /* check_suggested_username */ true);
  /// CheckTextFieldsSuggestedState(kAliceUsername, true, kBobPassword, true);
}

TEST_F(PasswordAutofillAgentTest, RestoresAfterJavaScriptModification) {
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  fake_driver_.reset_password_forms_calls();

  static const char script[] = "document.getElementById('username').value = ''";
  ExecuteJavaScriptForTests(script);
  CheckTextFieldsSuggestedState("", false, kAlicePassword, true);

  password_autofill_agent_->OnDynamicFormsSeen();
  CheckTextFieldsSuggestedState(kAliceUsername, false, kAlicePassword, true);

  EXPECT_FALSE(fake_driver_.called_password_forms_parsed());
  EXPECT_FALSE(fake_driver_.called_password_forms_rendered());
}

TEST_F(PasswordAutofillAgentTest, DoNotRestoreWhenFormStructureWasChanged) {
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsSuggestedState(kAliceUsername, true, kAlicePassword, true);

  static const char clear_username_script[] =
      "document.getElementById('username').value = ''";
  ExecuteJavaScriptForTests(clear_username_script);
  static const char add_input_element_script[] =
      "document.getElementById('LoginTestForm').appendChild(document."
      "createElement('input'))";
  ExecuteJavaScriptForTests(add_input_element_script);
  CheckTextFieldsSuggestedState("", false, kAlicePassword, true);

  password_autofill_agent_->OnDynamicFormsSeen();
  CheckTextFieldsSuggestedState("", false, kAlicePassword, true);
}

// Tests that a single username is filled and is exposed to JavaScript only
// after user gesture.
TEST_F(PasswordAutofillAgentTest, FillOnLoadSingleUsername) {
  // Simulate filling single username by clearing password fill data.
  ClearField(&fill_data_.password_field);

  SimulateOnFillPasswordForm(fill_data_);

  // The username should have been autofilled.
  CheckTextFieldsSuggestedState(kAliceUsername, true, std::string(), false);

  // However, it should have filled with the suggested value, it should not have
  // filled with DOM accessible value.
  CheckTextFieldsDOMState(std::string(), true, std::string(), false);

  // Simulate a user click so that the username field's real value is filled.
  SimulateElementClick(kUsernameName);
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), false);
}

// Tests that |PreviewSuggestion| properly previews the single username.
TEST_F(PasswordAutofillAgentTest, SingleUsernamePreviewSuggestion) {
  ClearField(&fill_data_.password_field);
  // Simulate the browser sending the login info, but set |wait_for_username| to
  // prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));
  CheckTextFieldsSuggestedState(kAliceUsername, true, std::string(), false);

  // Try previewing with a username different from the one that was initially
  // sent to the renderer.
  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kBobUsername, kCarolPassword));
  CheckTextFieldsSuggestedState(kBobUsername, true, std::string(), false);
}

// Tests that |FillSuggestion| properly fills the single username.
TEST_F(PasswordAutofillAgentTest, SingleUsernameFillSuggestion) {
  ClearField(&fill_data_.password_field);
  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // After filling with the suggestion, the username field should be filled.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      username_element_, ASCIIToUTF16(kAliceUsername),
      ASCIIToUTF16(kAlicePassword)));
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), false);
  int username_length = strlen(kAliceUsername);
  CheckUsernameSelection(username_length, username_length);

  // Try Filling with a suggestion with a username different from the one that
  // was initially sent to the renderer.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      username_element_, ASCIIToUTF16(kBobUsername),
      ASCIIToUTF16(kCarolPassword)));
  CheckTextFieldsDOMState(kBobUsername, true, std::string(), false);
  username_length = strlen(kBobUsername);
  CheckUsernameSelection(username_length, username_length);
}

// Tests that |ClearPreview| properly clears previewed single username.
TEST_F(PasswordAutofillAgentTest, SingleUsernameClearPreview) {
  ClearField(&fill_data_.password_field);
  ResetFieldState(&username_element_, "ali", WebAutofillState::kPreviewed);
  username_element_.SetSelectionRange(3, 3);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("ali", true, std::string(), false);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));
  EXPECT_TRUE(
      password_autofill_agent_->DidClearAutofillSelection(username_element_));

  EXPECT_TRUE(username_element_.SuggestedValue().IsEmpty());
  CheckTextFieldsDOMState("ali", true, std::string(), false);
  CheckUsernameSelection(3, 3);
}

// Fill on account select for credentials with empty usernames:
// Do not refill usernames if non-empty username is already selected.
TEST_F(PasswordAutofillAgentTest, NoUsernameCredential) {
  SimulateOnFillPasswordForm(fill_data_);
  ClearUsernameAndPasswordFields();
  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  SimulateSuggestionChoiceOfUsernameAndPassword(password_element_,
                                                ASCIIToUTF16(kAliceUsername),
                                                ASCIIToUTF16(kAlicePassword));
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(fake_driver_, ShowPasswordSuggestions);
  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, ASCIIToUTF16(kEmptyUsername),
      ASCIIToUTF16(kEmptyUsernamePassword));

  CheckTextFieldsDOMState(kAliceUsername, true, kEmptyUsernamePassword, true);
}

// Tests that any fields that have user input are not refilled on the next
// call of FillPasswordForm.
TEST_F(PasswordAutofillAgentTest, NoRefillOfUserInput) {
  ClearUsernameAndPasswordFields();
  SimulateOnFillPasswordForm(fill_data_);
  SimulateElementClick(kPasswordName);
  SimulatePasswordTyping("newpwd");
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsStateForElements(username_element_, kAliceUsername, true,
                                  password_element_, "newpwd", false, false,
                                  false);
}

}  // namespace autofill
