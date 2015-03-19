// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_autofill_agent.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAutofillClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

namespace autofill {
namespace {

// The size above which we stop triggering autocomplete.
static const size_t kMaximumTextSizeForAutocomplete = 1000;

// Experiment information
const char kFillOnAccountSelectFieldTrialName[] = "FillOnAccountSelect";
const char kFillOnAccountSelectFieldTrialEnabledWithHighlightGroup[] =
    "EnableWithHighlight";
const char kFillOnAccountSelectFieldTrialEnabledWithNoHighlightGroup[] =
    "EnableWithNoHighlight";

// Maps element names to the actual elements to simplify form filling.
typedef std::map<base::string16, blink::WebInputElement> FormInputElementMap;

// Use the shorter name when referencing SavePasswordProgressLogger::StringID
// values to spare line breaks. The code provides enough context for that
// already.
typedef SavePasswordProgressLogger Logger;

// Utility struct for form lookup and autofill. When we parse the DOM to look up
// a form, in addition to action and origin URL's we have to compare all
// necessary form elements. To avoid having to look these up again when we want
// to fill the form, the FindFormElements function stores the pointers
// in a FormElements* result, referenced to ensure they are safe to use.
struct FormElements {
  blink::WebFormElement form_element;
  FormInputElementMap input_elements;
};

typedef std::vector<FormElements*> FormElementsList;

bool FillDataContainsUsername(const PasswordFormFillData& fill_data) {
  return !fill_data.username_field.name.empty();
}

// Utility function to find the unique entry of the |form_element| for the
// specified input |field|. On successful find, adds it to |result| and returns
// |true|. Otherwise clears the references from each |HTMLInputElement| from
// |result| and returns |false|.
bool FindFormInputElement(blink::WebFormElement* form_element,
                          const FormFieldData& field,
                          FormElements* result) {
  blink::WebVector<blink::WebNode> temp_elements;
  form_element->getNamedElements(field.name, temp_elements);

  // Match the first input element, if any.
  // |getNamedElements| may return non-input elements where the names match,
  // so the results are filtered for input elements.
  // If more than one match is made, then we have ambiguity (due to misuse
  // of "name" attribute) so is it considered not found.
  bool found_input = false;
  for (size_t i = 0; i < temp_elements.size(); ++i) {
    if (temp_elements[i].to<blink::WebElement>().hasHTMLTagName("input")) {
      // Check for a non-unique match.
      if (found_input) {
        found_input = false;
        break;
      }

      // Only fill saved passwords into password fields and usernames into
      // text fields.
      blink::WebInputElement input_element =
          temp_elements[i].to<blink::WebInputElement>();
      if (input_element.isPasswordField() !=
          (field.form_control_type == "password"))
        continue;

      // This element matched, add it to our temporary result. It's possible
      // there are multiple matches, but for purposes of identifying the form
      // one suffices and if some function needs to deal with multiple
      // matching elements it can get at them through the FormElement*.
      // Note: This assignment adds a reference to the InputElement.
      result->input_elements[field.name] = input_element;
      found_input = true;
    }
  }

  // A required element was not found. This is not the right form.
  // Make sure no input elements from a partially matched form in this
  // iteration remain in the result set.
  // Note: clear will remove a reference from each InputElement.
  if (!found_input) {
    result->input_elements.clear();
    return false;
  }

  return true;
}

bool ShouldFillOnAccountSelect() {
  std::string group_name =
      base::FieldTrialList::FindFullName(kFillOnAccountSelectFieldTrialName);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableFillOnAccountSelect)) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFillOnAccountSelect) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFillOnAccountSelectNoHighlighting)) {
    return true;
  }

  return group_name ==
             kFillOnAccountSelectFieldTrialEnabledWithHighlightGroup ||
         group_name ==
             kFillOnAccountSelectFieldTrialEnabledWithNoHighlightGroup;
}

bool ShouldHighlightFields() {
  std::string group_name =
      base::FieldTrialList::FindFullName(kFillOnAccountSelectFieldTrialName);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableFillOnAccountSelect) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFillOnAccountSelect)) {
    return true;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFillOnAccountSelectNoHighlighting)) {
    return false;
  }

  return group_name !=
         kFillOnAccountSelectFieldTrialEnabledWithNoHighlightGroup;
}

// Helper to search the given form element for the specified input elements in
// |data|, and add results to |result|.
bool FindFormInputElements(blink::WebFormElement* form_element,
                           const PasswordFormFillData& data,
                           FormElements* result) {
  return FindFormInputElement(form_element, data.password_field, result) &&
         (!FillDataContainsUsername(data) ||
          FindFormInputElement(form_element, data.username_field, result));
}

// Helper to locate form elements identified by |data|.
void FindFormElements(content::RenderFrame* render_frame,
                      const PasswordFormFillData& data,
                      FormElementsList* results) {
  DCHECK(results);

  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();

  blink::WebDocument doc = render_frame->GetWebFrame()->document();
  if (!doc.isHTMLDocument())
    return;

  GURL full_origin(doc.url());
  if (data.origin != full_origin.ReplaceComponents(rep))
    return;

  blink::WebVector<blink::WebFormElement> forms;
  doc.forms(forms);

  for (size_t i = 0; i < forms.size(); ++i) {
    blink::WebFormElement fe = forms[i];

    GURL full_action(doc.completeURL(fe.action()));
    if (full_action.is_empty()) {
      // The default action URL is the form's origin.
      full_action = full_origin;
    }

    // Action URL must match.
    if (data.action != full_action.ReplaceComponents(rep))
      continue;

    scoped_ptr<FormElements> curr_elements(new FormElements);
    if (!FindFormInputElements(&fe, data, curr_elements.get()))
      continue;

    // We found the right element.
    // Note: this assignment adds a reference to |fe|.
    curr_elements->form_element = fe;
    results->push_back(curr_elements.release());
  }
}

bool IsElementEditable(const blink::WebInputElement& element) {
  return element.isEnabled() && !element.isReadOnly();
}

bool DoUsernamesMatch(const base::string16& username1,
                      const base::string16& username2,
                      bool exact_match) {
  if (exact_match)
    return username1 == username2;
  return StartsWith(username1, username2, true);
}

// Returns |true| if the given element is editable. Otherwise, returns |false|.
bool IsElementAutocompletable(const blink::WebInputElement& element) {
  return IsElementEditable(element);
}

// Returns true if the password specified in |form| is a default value.
bool PasswordValueIsDefault(const base::string16& password_element,
                            const base::string16& password_value,
                            blink::WebFormElement form_element) {
  blink::WebVector<blink::WebNode> temp_elements;
  form_element.getNamedElements(password_element, temp_elements);

  // We are loose in our definition here and will return true if any of the
  // appropriately named elements match the element to be saved. Currently
  // we ignore filling passwords where naming is ambigious anyway.
  for (size_t i = 0; i < temp_elements.size(); ++i) {
    if (temp_elements[i].to<blink::WebElement>().getAttribute("value") ==
        password_value)
      return true;
  }
  return false;
}

// Return true if either password_value or new_password_value is not empty and
// not default.
bool FormContainsNonDefaultPasswordValue(const PasswordForm& password_form,
                                         blink::WebFormElement form_element) {
  return (!password_form.password_value.empty() &&
          !PasswordValueIsDefault(password_form.password_element,
                                  password_form.password_value,
                                  form_element)) ||
      (!password_form.new_password_value.empty() &&
       !PasswordValueIsDefault(password_form.new_password_element,
                               password_form.new_password_value,
                               form_element));
}

// Log a message including the name, method and action of |form|.
void LogHTMLForm(SavePasswordProgressLogger* logger,
                 SavePasswordProgressLogger::StringID message_id,
                 const blink::WebFormElement& form) {
  logger->LogHTMLForm(message_id,
                      form.name().utf8(),
                      GURL(form.action().utf8()));
}

// Sets |suggestions_present| to true if there are any suggestions to be derived
// from |fill_data|. Unless |show_all| is true, only considers suggestions with
// usernames having |current_username| as a prefix. Returns true if a username
// from the |fill_data.other_possible_usernames| would be included in the
// suggestions.
bool GetSuggestionsStats(const PasswordFormFillData& fill_data,
                         const base::string16& current_username,
                         bool show_all,
                         bool* suggestions_present) {
  *suggestions_present = false;

  for (const auto& usernames : fill_data.other_possible_usernames) {
    for (size_t i = 0; i < usernames.second.size(); ++i) {
      if (show_all ||
          StartsWith(usernames.second[i], current_username, false)) {
        *suggestions_present = true;
        return true;
      }
    }
  }

  if (show_all ||
      StartsWith(fill_data.username_field.value, current_username, false)) {
    *suggestions_present = true;
    return false;
  }

  for (const auto& login : fill_data.additional_logins) {
    if (show_all || StartsWith(login.first, current_username, false)) {
      *suggestions_present = true;
      return false;
    }
  }

  return false;
}

// Returns true if there exists a credential suggestion whose username field is
// an exact match to the current username (not just a prefix).
bool HasExactMatchSuggestion(const PasswordFormFillData& fill_data,
                             const base::string16& current_username) {
  if (fill_data.username_field.value == current_username)
    return true;

  for (const auto& usernames : fill_data.other_possible_usernames) {
    for (const auto& username_string : usernames.second) {
      if (username_string == current_username)
        return true;
    }
  }

  for (const auto& login : fill_data.additional_logins) {
    if (login.first == current_username)
      return true;
  }

  return false;
}

// This function attempts to fill |username_element| and |password_element|
// with values from |fill_data|. The |password_element| will only have the
// |suggestedValue| set, and will be registered for copying that to the real
// value through |registration_callback|. The function returns true when
// selected username comes from |fill_data.other_possible_usernames|. |options|
// should be a bitwise mask of FillUserNameAndPasswordOptions values.
bool FillUserNameAndPassword(
    blink::WebInputElement* username_element,
    blink::WebInputElement* password_element,
    const PasswordFormFillData& fill_data,
    bool exact_username_match,
    bool set_selection,
    std::map<const blink::WebInputElement, blink::WebString>&
        nonscript_modified_values,
    base::Callback<void(blink::WebInputElement*)> registration_callback) {
  bool other_possible_username_selected = false;
  // Don't fill username if password can't be set.
  if (!IsElementAutocompletable(*password_element))
    return false;

  base::string16 current_username;
  if (!username_element->isNull()) {
    current_username = username_element->value();
  }

  // username and password will contain the match found if any.
  base::string16 username;
  base::string16 password;

  // Look for any suitable matches to current field text.
  if (DoUsernamesMatch(fill_data.username_field.value, current_username,
                       exact_username_match)) {
    username = fill_data.username_field.value;
    password = fill_data.password_field.value;
  } else {
    // Scan additional logins for a match.
    PasswordFormFillData::LoginCollection::const_iterator iter;
    for (iter = fill_data.additional_logins.begin();
         iter != fill_data.additional_logins.end();
         ++iter) {
      if (DoUsernamesMatch(
              iter->first, current_username, exact_username_match)) {
        username = iter->first;
        password = iter->second.password;
        break;
      }
    }

    // Check possible usernames.
    if (username.empty() && password.empty()) {
      for (PasswordFormFillData::UsernamesCollection::const_iterator iter =
               fill_data.other_possible_usernames.begin();
           iter != fill_data.other_possible_usernames.end();
           ++iter) {
        for (size_t i = 0; i < iter->second.size(); ++i) {
          if (DoUsernamesMatch(
                  iter->second[i], current_username, exact_username_match)) {
            other_possible_username_selected = true;
            username = iter->second[i];
            password = iter->first.password;
            break;
          }
        }
        if (!username.empty() && !password.empty())
          break;
      }
    }
  }
  if (password.empty())
    return other_possible_username_selected;  // No match was found.

  // TODO(tkent): Check maxlength and pattern for both username and password
  // fields.

  // Input matches the username, fill in required values.
  if (!username_element->isNull() &&
      IsElementAutocompletable(*username_element)) {
    username_element->setValue(username, true);
    nonscript_modified_values[*username_element] = username;
    username_element->setAutofilled(true);

    if (set_selection) {
      username_element->setSelectionRange(current_username.length(),
                                          username.length());
    }
  } else if (current_username != username) {
    // If the username can't be filled and it doesn't match a saved password
    // as is, don't autofill a password.
    return other_possible_username_selected;
  }

  // Wait to fill in the password until a user gesture occurs. This is to make
  // sure that we do not fill in the DOM with a password until we believe the
  // user is intentionally interacting with the page.
  password_element->setSuggestedValue(password);
  nonscript_modified_values[*password_element] = password;
  registration_callback.Run(password_element);

  password_element->setAutofilled(true);
  return other_possible_username_selected;
}

// Attempts to fill |username_element| and |password_element| with the
// |fill_data|. Will use the data corresponding to the preferred username,
// unless the |username_element| already has a value set. In that case,
// attempts to fill the password matching the already filled username, if
// such a password exists. The |password_element| will have the
// |suggestedValue| set, and |suggestedValue| will be registered for copying to
// the real value through |registration_callback|. Returns true when the
// username gets selected from |other_possible_usernames|, else returns false.
bool FillFormOnPasswordReceived(
    const PasswordFormFillData& fill_data,
    blink::WebInputElement username_element,
    blink::WebInputElement password_element,
    std::map<const blink::WebInputElement, blink::WebString>&
        nonscript_modified_values,
    base::Callback<void(blink::WebInputElement*)> registration_callback) {
  // Do not fill if the password field is in an iframe.
  DCHECK(password_element.document().frame());
  if (password_element.document().frame()->parent())
    return false;

  // If we can't modify the password, don't try to set the username
  if (!IsElementAutocompletable(password_element))
    return false;

  bool form_contains_username_field = FillDataContainsUsername(fill_data);
  // If the form contains an autocompletable username field, try to set the
  // username to the preferred name, but only if:
  //   (a) The fill-on-account-select flag is not set, and
  //   (b) The username element isn't prefilled
  //
  // If (a) is false, then just mark the username element as autofilled if the
  // user is not in the "no highlighting" group and return so the fill step is
  // skipped.
  //
  // If there is no autocompletable username field, and (a) is false, then the
  // username element cannot be autofilled, but the user should still be able to
  // select to fill the password element, so the password element must be marked
  // as autofilled and the fill step should also be skipped if the user is not
  // in the "no highlighting" group.
  //
  // In all other cases, do nothing.
  bool form_has_fillable_username = form_contains_username_field &&
                                    IsElementAutocompletable(username_element);

  if (ShouldFillOnAccountSelect()) {
    if (!ShouldHighlightFields()) {
      return false;
    }

    if (form_has_fillable_username) {
      username_element.setAutofilled(true);
    } else if (username_element.isNull() ||
               HasExactMatchSuggestion(fill_data, username_element.value())) {
      password_element.setAutofilled(true);
    }
    return false;
  }

  if (form_has_fillable_username && username_element.value().isEmpty()) {
    // TODO(tkent): Check maxlength and pattern.
    username_element.setValue(fill_data.username_field.value, true);
  }

  // Fill if we have an exact match for the username. Note that this sets
  // username to autofilled.
  return FillUserNameAndPassword(&username_element,
                                 &password_element,
                                 fill_data,
                                 true /* exact_username_match */,
                                 false /* set_selection */,
                                 nonscript_modified_values,
                                 registration_callback);
}

// Takes a |map| with pointers as keys and linked_ptr as values, and returns
// true if |key| is not NULL and  |map| contains a non-NULL entry for |key|.
// Makes sure not to create an entry as a side effect of using the operator [].
template <class Key, class Value>
bool ContainsNonNullEntryForNonNullKey(
    const std::map<Key*, linked_ptr<Value>>& map,
    Key* key) {
  if (!key)
    return false;
  auto it = map.find(key);
  return it != map.end() && it->second.get();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, public:

PasswordAutofillAgent::PasswordAutofillAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      legacy_(render_frame->GetRenderView(), this),
      usernames_usage_(NOTHING_TO_AUTOFILL),
      logging_state_active_(false),
      was_username_autofilled_(false),
      was_password_autofilled_(false),
      username_selection_start_(0),
      did_stop_loading_(false),
      weak_ptr_factory_(this) {
  save_password_on_in_page_navigation_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          autofill::switches::kEnablePasswordSaveOnInPageNavigation);
  Send(new AutofillHostMsg_PasswordAutofillAgentConstructed(routing_id()));
}

PasswordAutofillAgent::~PasswordAutofillAgent() {
}

PasswordAutofillAgent::PasswordValueGatekeeper::PasswordValueGatekeeper()
    : was_user_gesture_seen_(false) {
}

PasswordAutofillAgent::PasswordValueGatekeeper::~PasswordValueGatekeeper() {
}

void PasswordAutofillAgent::PasswordValueGatekeeper::RegisterElement(
    blink::WebInputElement* element) {
  if (was_user_gesture_seen_)
    ShowValue(element);
  else
    elements_.push_back(*element);
}

void PasswordAutofillAgent::PasswordValueGatekeeper::OnUserGesture() {
  was_user_gesture_seen_ = true;

  for (std::vector<blink::WebInputElement>::iterator it = elements_.begin();
       it != elements_.end();
       ++it) {
    ShowValue(&(*it));
  }

  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::Reset() {
  was_user_gesture_seen_ = false;
  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::ShowValue(
    blink::WebInputElement* element) {
  if (!element->isNull() && !element->suggestedValue().isEmpty())
    element->setValue(element->suggestedValue(), true);
}

bool PasswordAutofillAgent::TextFieldDidEndEditing(
    const blink::WebInputElement& element) {
  LoginToPasswordInfoMap::const_iterator iter =
      login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  const PasswordInfo& password_info = iter->second;
  // Don't let autofill overwrite an explicit change made by the user.
  if (password_info.password_was_edited_last)
    return false;

  const PasswordFormFillData& fill_data = password_info.fill_data;

  // If wait_for_username is false, we should have filled when the text changed.
  if (!fill_data.wait_for_username)
    return false;

  blink::WebInputElement password = password_info.password_field;
  if (!IsElementEditable(password))
    return false;

  blink::WebInputElement username = element;  // We need a non-const.

  // Do not set selection when ending an editing session, otherwise it can
  // mess with focus.
  if (FillUserNameAndPassword(
          &username, &password, fill_data, true, false,
          nonscript_modified_values_,
          base::Bind(&PasswordValueGatekeeper::RegisterElement,
                     base::Unretained(&gatekeeper_)))) {
    usernames_usage_ = OTHER_POSSIBLE_USERNAME_SELECTED;
  }
  return true;
}

bool PasswordAutofillAgent::TextDidChangeInTextField(
    const blink::WebInputElement& element) {
  // TODO(vabr): Get a mutable argument instead. http://crbug.com/397083
  blink::WebInputElement mutable_element = element;  // We need a non-const.

  if (element.isTextField())
    nonscript_modified_values_[element] = element.value();

  DCHECK_EQ(element.document().frame(), render_frame()->GetWebFrame());

  if (element.isPasswordField()) {
    // Some login forms have event handlers that put a hash of the password into
    // a hidden field and then clear the password (http://crbug.com/28910,
    // http://crbug.com/391693). This method gets called before any of those
    // handlers run, so save away a copy of the password in case it gets lost.
    // To honor the user having explicitly cleared the password, even an empty
    // password will be saved here.
    ProvisionallySavePassword(element.form(), RESTRICTION_NONE);

    PasswordToLoginMap::iterator iter = password_to_username_.find(element);
    if (iter != password_to_username_.end()) {
      login_to_password_info_[iter->second].password_was_edited_last = true;
      // Note that the suggested value of |mutable_element| was reset when its
      // value changed.
      mutable_element.setAutofilled(false);
    }
    return false;
  }

  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  // The input text is being changed, so any autofilled password is now
  // outdated.
  mutable_element.setAutofilled(false);
  iter->second.password_was_edited_last = false;

  blink::WebInputElement password = iter->second.password_field;
  if (password.isAutofilled()) {
    password.setValue(base::string16(), true);
    password.setAutofilled(false);
  }

  // If wait_for_username is true we will fill when the username loses focus.
  if (iter->second.fill_data.wait_for_username)
    return false;

  if (!element.isText() || !IsElementAutocompletable(element) ||
      !IsElementAutocompletable(password)) {
    return false;
  }

  // Don't inline autocomplete if the user is deleting, that would be confusing.
  // But refresh the popup.  Note, since this is ours, return true to signal
  // no further processing is required.
  if (iter->second.backspace_pressed_last) {
    ShowSuggestionPopup(iter->second.fill_data, element, false, false);
    return true;
  }

  blink::WebString name = element.nameForAutofill();
  if (name.isEmpty())
    return false;  // If the field has no name, then we won't have values.

  // Don't attempt to autofill with values that are too large.
  if (element.value().length() > kMaximumTextSizeForAutocomplete)
    return false;

  // The caret position should have already been updated.
  PerformInlineAutocomplete(element, password, iter->second.fill_data);
  return true;
}

bool PasswordAutofillAgent::TextFieldHandlingKeyDown(
    const blink::WebInputElement& element,
    const blink::WebKeyboardEvent& event) {
  // If using the new Autofill UI that lives in the browser, it will handle
  // keypresses before this function. This is not currently an issue but if
  // the keys handled there or here change, this issue may appear.

  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  int win_key_code = event.windowsKeyCode;
  iter->second.backspace_pressed_last =
      (win_key_code == ui::VKEY_BACK || win_key_code == ui::VKEY_DELETE);
  return true;
}

bool PasswordAutofillAgent::FillSuggestion(
    const blink::WebNode& node,
    const blink::WebString& username,
    const blink::WebString& password) {
  // The element in context of the suggestion popup.
  blink::WebInputElement filled_element;
  PasswordInfo* password_info;

  if (!FindLoginInfo(node, &filled_element, &password_info) ||
      !IsElementAutocompletable(filled_element) ||
      !IsElementAutocompletable(password_info->password_field)) {
    return false;
  }

  password_info->password_was_edited_last = false;
  // Note that in cases where filled_element is the password element, the value
  // gets overwritten with the correct one below.
  filled_element.setValue(username, true);
  filled_element.setAutofilled(true);

  password_info->password_field.setValue(password, true);
  password_info->password_field.setAutofilled(true);

  filled_element.setSelectionRange(filled_element.value().length(),
                                   filled_element.value().length());

  return true;
}

bool PasswordAutofillAgent::PreviewSuggestion(
    const blink::WebNode& node,
    const blink::WebString& username,
    const blink::WebString& password) {
  blink::WebInputElement username_element;
  PasswordInfo* password_info;

  if (!FindLoginInfo(node, &username_element, &password_info) ||
      !IsElementAutocompletable(username_element) ||
      !IsElementAutocompletable(password_info->password_field)) {
    return false;
  }

  was_username_autofilled_ = username_element.isAutofilled();
  username_selection_start_ = username_element.selectionStart();
  username_element.setSuggestedValue(username);
  username_element.setAutofilled(true);
  username_element.setSelectionRange(
      username_selection_start_,
      username_element.suggestedValue().length());

  was_password_autofilled_ = password_info->password_field.isAutofilled();
  password_info->password_field.setSuggestedValue(password);
  password_info->password_field.setAutofilled(true);

  return true;
}

bool PasswordAutofillAgent::DidClearAutofillSelection(
    const blink::WebNode& node) {
  blink::WebInputElement username_element;
  PasswordInfo* password_info;
  if (!FindLoginInfo(node, &username_element, &password_info))
    return false;

  ClearPreview(&username_element, &password_info->password_field);
  return true;
}

bool PasswordAutofillAgent::FindPasswordInfoForElement(
    const blink::WebInputElement& element,
    const blink::WebInputElement** username_element,
    PasswordInfo** password_info) {
  DCHECK(username_element && password_info);
  if (!element.isPasswordField()) {
    *username_element = &element;
  } else {
    PasswordToLoginMap::const_iterator password_iter =
        password_to_username_.find(element);
    if (password_iter == password_to_username_.end())
      return false;
    *username_element = &password_iter->second;
  }

  LoginToPasswordInfoMap::iterator iter =
      login_to_password_info_.find(**username_element);

  if (iter == login_to_password_info_.end())
    return false;

  *password_info = &iter->second;
  return true;
}

bool PasswordAutofillAgent::ShowSuggestions(
    const blink::WebInputElement& element,
    bool show_all) {
  const blink::WebInputElement* username_element;
  PasswordInfo* password_info;
  if (!FindPasswordInfoForElement(element, &username_element, &password_info))
    return false;

  // If autocomplete='off' is set on the form elements, no suggestion dialog
  // should be shown. However, return |true| to indicate that this is a known
  // password form and that the request to show suggestions has been handled (as
  // a no-op).
  if (!IsElementAutocompletable(element) ||
      !IsElementAutocompletable(password_info->password_field))
    return true;

  bool username_is_available =
      !username_element->isNull() && IsElementEditable(*username_element);
  // If the element is a password field, a popup should only be shown if there
  // is no username or the corresponding username element is not editable since
  // it is only in that case that the username element does not have a
  // suggestions popup.
  if (element.isPasswordField() && username_is_available)
    return true;

  // Chrome should never show more than one account for a password element since
  // this implies that the username element cannot be modified. Thus even if
  // |show_all| is true, check if the element in question is a password element
  // for the call to ShowSuggestionPopup.
  return ShowSuggestionPopup(
      password_info->fill_data,
      username_element->isNull() ? element : *username_element,
      show_all && !element.isPasswordField(), element.isPasswordField());
}

bool PasswordAutofillAgent::OriginCanAccessPasswordManager(
    const blink::WebSecurityOrigin& origin) {
  return origin.canAccessPasswordManager();
}

void PasswordAutofillAgent::OnDynamicFormsSeen() {
  SendPasswordForms(false /* only_visible */);
}

void PasswordAutofillAgent::XHRSucceeded() {
  if (!ProvisionallySavedPasswordIsValid())
    return;

  // Prompt to save only if the form is now gone, either invisible or
  // removed from the DOM.
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  blink::WebVector<blink::WebFormElement> forms;
  frame->document().forms(forms);

  for (size_t i = 0; i < forms.size(); ++i) {
    const blink::WebFormElement& form = forms[i];
    if (!IsWebNodeVisible(form)) {
      continue;
    }

    scoped_ptr<PasswordForm> password_form(
        CreatePasswordForm(form, &nonscript_modified_values_));
    if (password_form.get()) {
      if (provisionally_saved_form_->action == password_form->action) {
        // Form still exists, no save required.
        return;
      }
    }
  }
  Send(new AutofillHostMsg_InPageNavigation(routing_id(),
                                            *provisionally_saved_form_));
  provisionally_saved_form_.reset();
}

void PasswordAutofillAgent::FirstUserGestureObserved() {
  gatekeeper_.OnUserGesture();
}

void PasswordAutofillAgent::SendPasswordForms(bool only_visible) {
  scoped_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(this, routing_id()));
    logger->LogMessage(Logger::STRING_SEND_PASSWORD_FORMS_METHOD);
    logger->LogBoolean(Logger::STRING_ONLY_VISIBLE, only_visible);
  }

  blink::WebFrame* frame = render_frame()->GetWebFrame();
  // Make sure that this security origin is allowed to use password manager.
  blink::WebSecurityOrigin origin = frame->document().securityOrigin();
  if (logger) {
    logger->LogURL(Logger::STRING_SECURITY_ORIGIN,
                   GURL(origin.toString().utf8()));
  }
  if (!OriginCanAccessPasswordManager(origin)) {
    if (logger) {
      logger->LogMessage(Logger::STRING_SECURITY_ORIGIN_FAILURE);
    }
    return;
  }

  // Checks whether the webpage is a redirect page or an empty page.
  if (IsWebpageEmpty(frame)) {
    if (logger) {
      logger->LogMessage(Logger::STRING_WEBPAGE_EMPTY);
    }
    return;
  }

  blink::WebVector<blink::WebFormElement> forms;
  frame->document().forms(forms);
  if (logger)
    logger->LogNumber(Logger::STRING_NUMBER_OF_ALL_FORMS, forms.size());

  std::vector<PasswordForm> password_forms;
  for (size_t i = 0; i < forms.size(); ++i) {
    const blink::WebFormElement& form = forms[i];
    if (only_visible) {
      bool is_form_visible = IsWebNodeVisible(form);
      if (logger) {
        LogHTMLForm(logger.get(), Logger::STRING_FORM_FOUND_ON_PAGE, form);
        logger->LogBoolean(Logger::STRING_FORM_IS_VISIBLE, is_form_visible);
      }

      // If requested, ignore non-rendered forms, e.g., those styled with
      // display:none.
      if (!is_form_visible)
        continue;
    }

    scoped_ptr<PasswordForm> password_form(CreatePasswordForm(form, nullptr));
    if (password_form.get()) {
      if (logger) {
        logger->LogPasswordForm(Logger::STRING_FORM_IS_PASSWORD,
                                *password_form);
      }
      password_forms.push_back(*password_form);
    }
  }

  if (password_forms.empty() && !only_visible) {
    // We need to send the PasswordFormsRendered message regardless of whether
    // there are any forms visible, as this is also the code path that triggers
    // showing the infobar.
    return;
  }

  if (only_visible) {
    Send(new AutofillHostMsg_PasswordFormsRendered(routing_id(),
                                                   password_forms,
                                                   did_stop_loading_));
  } else {
    Send(new AutofillHostMsg_PasswordFormsParsed(routing_id(), password_forms));
  }
}

bool PasswordAutofillAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordAutofillAgent, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_FillPasswordForm, OnFillPasswordForm)
    IPC_MESSAGE_HANDLER(AutofillMsg_SetLoggingState, OnSetLoggingState)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PasswordAutofillAgent::DidFinishDocumentLoad() {
  // The |frame| contents have been parsed, but not yet rendered.  Let the
  // PasswordManager know that forms are loaded, even though we can't yet tell
  // whether they're visible.
  SendPasswordForms(false);
}

void PasswordAutofillAgent::DidFinishLoad() {
  // The |frame| contents have been rendered.  Let the PasswordManager know
  // which of the loaded frames are actually visible to the user.  This also
  // triggers the "Save password?" infobar if the user just submitted a password
  // form.
  SendPasswordForms(true);
}

void PasswordAutofillAgent::FrameWillClose() {
  FrameClosing();
}

void PasswordAutofillAgent::DidCommitProvisionalLoad(
    bool is_new_navigation, bool is_same_page_navigation) {
  if (!save_password_on_in_page_navigation_)
    return;
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  // TODO(dvadym): check if we need to check if it is main frame navigation
  // http://crbug.com/443155
  if (frame->parent())
    return; // Not a top-level navigation.

  if (is_same_page_navigation && provisionally_saved_form_) {
    Send(new AutofillHostMsg_InPageNavigation(routing_id(),
                                              *provisionally_saved_form_));
    provisionally_saved_form_.reset();
  }
}

void PasswordAutofillAgent::DidStartLoading() {
  did_stop_loading_ = false;
  if (usernames_usage_ != NOTHING_TO_AUTOFILL) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.OtherPossibleUsernamesUsage",
                              usernames_usage_, OTHER_POSSIBLE_USERNAMES_MAX);
    usernames_usage_ = NOTHING_TO_AUTOFILL;
  }
}

void PasswordAutofillAgent::DidStopLoading() {
  did_stop_loading_ = true;
}

void PasswordAutofillAgent::FrameDetached() {
  FrameClosing();
}

void PasswordAutofillAgent::WillSendSubmitEvent(
    const blink::WebFormElement& form) {
  // Forms submitted via XHR are not seen by WillSubmitForm if the default
  // onsubmit handler is overridden. Such submission first gets detected in
  // DidStartProvisionalLoad, which no longer knows about the particular form,
  // and uses the candidate stored in |provisionally_saved_form_|.
  //
  // User-typed password will get stored to |provisionally_saved_form_| in
  // TextDidChangeInTextField. Autofilled or JavaScript-copied passwords need to
  // be saved here.
  //
  // Only non-empty passwords are saved here. Empty passwords were likely
  // cleared by some scripts (http://crbug.com/28910, http://crbug.com/391693).
  // Had the user cleared the password, |provisionally_saved_form_| would
  // already have been updated in TextDidChangeInTextField.
  ProvisionallySavePassword(form, RESTRICTION_NON_EMPTY_PASSWORD);
}

void PasswordAutofillAgent::WillSubmitForm(const blink::WebFormElement& form) {
  scoped_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(this, routing_id()));
    logger->LogMessage(Logger::STRING_WILL_SUBMIT_FORM_METHOD);
    LogHTMLForm(logger.get(), Logger::STRING_HTML_FORM_FOR_SUBMIT, form);
  }

  scoped_ptr<PasswordForm> submitted_form = CreatePasswordForm(form, nullptr);

  // If there is a provisionally saved password, copy over the previous
  // password value so we get the user's typed password, not the value that
  // may have been transformed for submit.
  // TODO(gcasto): Do we need to have this action equality check? Is it trying
  // to prevent accidentally copying over passwords from a different form?
  if (submitted_form) {
    if (logger) {
      logger->LogPasswordForm(Logger::STRING_CREATED_PASSWORD_FORM,
                              *submitted_form);
    }
    if (provisionally_saved_form_ &&
        submitted_form->action == provisionally_saved_form_->action) {
      if (logger)
        logger->LogMessage(Logger::STRING_SUBMITTED_PASSWORD_REPLACED);
      submitted_form->password_value =
          provisionally_saved_form_->password_value;
      submitted_form->new_password_value =
          provisionally_saved_form_->new_password_value;
      submitted_form->username_value =
          provisionally_saved_form_->username_value;
    }

    // Some observers depend on sending this information now instead of when
    // the frame starts loading. If there are redirects that cause a new
    // RenderView to be instantiated (such as redirects to the WebStore)
    // we will never get to finish the load.
    Send(new AutofillHostMsg_PasswordFormSubmitted(routing_id(),
                                                   *submitted_form));
    provisionally_saved_form_.reset();
  } else if (logger) {
    logger->LogMessage(Logger::STRING_FORM_IS_NOT_PASSWORD);
  }
}

void PasswordAutofillAgent::LegacyDidStartProvisionalLoad(
    blink::WebLocalFrame* navigated_frame) {
  scoped_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(this, routing_id()));
    logger->LogMessage(Logger::STRING_DID_START_PROVISIONAL_LOAD_METHOD);
  }

  if (navigated_frame->parent()) {
    if (logger)
      logger->LogMessage(Logger::STRING_FRAME_NOT_MAIN_FRAME);
    return;
  }

  // Bug fix for crbug.com/368690. isProcessingUserGesture() is false when
  // the user is performing actions outside the page (e.g. typed url,
  // history navigation). We don't want to trigger saving in these cases.
  content::DocumentState* document_state =
      content::DocumentState::FromDataSource(
          navigated_frame->provisionalDataSource());
  content::NavigationState* navigation_state =
      document_state->navigation_state();
  ui::PageTransition type = navigation_state->GetTransitionType();
  if (ui::PageTransitionIsWebTriggerable(type) &&
      ui::PageTransitionIsNewNavigation(type) &&
      !blink::WebUserGestureIndicator::isProcessingUserGesture()) {
    // If onsubmit has been called, try and save that form.
    if (provisionally_saved_form_) {
      if (logger) {
        logger->LogPasswordForm(
            Logger::STRING_PROVISIONALLY_SAVED_FORM_FOR_FRAME,
            *provisionally_saved_form_);
      }
      Send(new AutofillHostMsg_PasswordFormSubmitted(
          routing_id(), *provisionally_saved_form_));
      provisionally_saved_form_.reset();
    } else {
      // Loop through the forms on the page looking for one that has been
      // filled out. If one exists, try and save the credentials.
      blink::WebVector<blink::WebFormElement> forms;
      render_frame()->GetWebFrame()->document().forms(forms);

      bool password_forms_found = false;
      for (size_t i = 0; i < forms.size(); ++i) {
        blink::WebFormElement form_element = forms[i];
        if (logger) {
          LogHTMLForm(logger.get(), Logger::STRING_FORM_FOUND_ON_PAGE,
                      form_element);
        }
        scoped_ptr<PasswordForm> password_form(
            CreatePasswordForm(form_element, &nonscript_modified_values_));
        if (password_form.get() && !password_form->username_value.empty() &&
            FormContainsNonDefaultPasswordValue(*password_form, form_element)) {
          password_forms_found = true;
          if (logger) {
            logger->LogPasswordForm(Logger::STRING_PASSWORD_FORM_FOUND_ON_PAGE,
                                    *password_form);
          }
          Send(new AutofillHostMsg_PasswordFormSubmitted(routing_id(),
                                                         *password_form));
        }
      }
      if (!password_forms_found && logger)
        logger->LogMessage(Logger::STRING_PASSWORD_FORM_NOT_FOUND_ON_PAGE);
    }
  }

  // This is a new navigation, so require a new user gesture before filling in
  // passwords.
  gatekeeper_.Reset();
}

void PasswordAutofillAgent::OnFillPasswordForm(
    int key,
    const PasswordFormFillData& form_data) {
  if (usernames_usage_ == NOTHING_TO_AUTOFILL) {
    if (form_data.other_possible_usernames.size())
      usernames_usage_ = OTHER_POSSIBLE_USERNAMES_PRESENT;
    else if (usernames_usage_ == NOTHING_TO_AUTOFILL)
      usernames_usage_ = OTHER_POSSIBLE_USERNAMES_ABSENT;
  }

  FormElementsList forms;
  // We own the FormElements* in forms.
  FindFormElements(render_frame(), form_data, &forms);
  FormElementsList::iterator iter;
  for (iter = forms.begin(); iter != forms.end(); ++iter) {
    scoped_ptr<FormElements> form_elements(*iter);

    // Attach autocomplete listener to enable selecting alternate logins.
    blink::WebInputElement username_element, password_element;

    // Check whether the password form has a username input field.
    bool form_contains_username_field = FillDataContainsUsername(form_data);
    if (form_contains_username_field) {
      username_element =
          form_elements->input_elements[form_data.username_field.name];
    }

    // No password field, bail out.
    if (form_data.password_field.name.empty())
      break;

    // We might have already filled this form if there are two <form> elements
    // with identical markup.
    if (login_to_password_info_.find(username_element) !=
        login_to_password_info_.end())
      continue;

    // Get pointer to password element. (We currently only support single
    // password forms).
    password_element =
        form_elements->input_elements[form_data.password_field.name];

    // If wait_for_username is true, we don't want to initially fill the form
    // until the user types in a valid username.
    if (!form_data.wait_for_username &&
        FillFormOnPasswordReceived(
            form_data,
            username_element,
            password_element,
            nonscript_modified_values_,
            base::Bind(&PasswordValueGatekeeper::RegisterElement,
                       base::Unretained(&gatekeeper_)))) {
      usernames_usage_ = OTHER_POSSIBLE_USERNAME_SELECTED;
    }

    PasswordInfo password_info;
    password_info.fill_data = form_data;
    password_info.password_field = password_element;
    login_to_password_info_[username_element] = password_info;
    password_to_username_[password_element] = username_element;
    login_to_password_info_key_[username_element] = key;
  }
}

void PasswordAutofillAgent::OnSetLoggingState(bool active) {
  logging_state_active_ = active;
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, private:

PasswordAutofillAgent::PasswordInfo::PasswordInfo()
    : backspace_pressed_last(false), password_was_edited_last(false) {
}

bool PasswordAutofillAgent::ShowSuggestionPopup(
    const PasswordFormFillData& fill_data,
    const blink::WebInputElement& user_input,
    bool show_all,
    bool show_on_password_field) {
  DCHECK(!user_input.isNull());
  blink::WebFrame* frame = user_input.document().frame();
  if (!frame)
    return false;

  blink::WebView* webview = frame->view();
  if (!webview)
    return false;

  FormData form;
  FormFieldData field;
  FindFormAndFieldForFormControlElement(
      user_input, &form, &field, REQUIRE_NONE);

  blink::WebInputElement selected_element = user_input;
  if (show_on_password_field && !selected_element.isPasswordField()) {
    LoginToPasswordInfoMap::const_iterator iter =
        login_to_password_info_.find(user_input);
    DCHECK(iter != login_to_password_info_.end());
    selected_element = iter->second.password_field;
  }
  gfx::Rect bounding_box(selected_element.boundsInViewportSpace());

  blink::WebInputElement username;
  if (!show_on_password_field || !user_input.isPasswordField()) {
    username = user_input;
  }
  LoginToPasswordInfoKeyMap::const_iterator key_it =
      login_to_password_info_key_.find(username);
  DCHECK(key_it != login_to_password_info_key_.end());

  float scale =
      render_frame()->GetRenderView()->GetWebView()->pageScaleFactor();
  gfx::RectF bounding_box_scaled(bounding_box.x() * scale,
                                 bounding_box.y() * scale,
                                 bounding_box.width() * scale,
                                 bounding_box.height() * scale);
  int options = 0;
  if (show_all)
    options |= SHOW_ALL;
  if (show_on_password_field)
    options |= IS_PASSWORD_FIELD;
  base::string16 username_string(
      username.isNull() ? base::string16()
                        : static_cast<base::string16>(user_input.value()));
  Send(new AutofillHostMsg_ShowPasswordSuggestions(
      routing_id(), key_it->second, field.text_direction, username_string,
      options, bounding_box_scaled));

  bool suggestions_present = false;
  if (GetSuggestionsStats(fill_data, username_string, show_all,
                          &suggestions_present)) {
    usernames_usage_ = OTHER_POSSIBLE_USERNAME_SHOWN;
  }
  return suggestions_present;
}

void PasswordAutofillAgent::PerformInlineAutocomplete(
    const blink::WebInputElement& username_input,
    const blink::WebInputElement& password_input,
    const PasswordFormFillData& fill_data) {
  DCHECK(!fill_data.wait_for_username);

  // We need non-const versions of the username and password inputs.
  blink::WebInputElement username = username_input;
  blink::WebInputElement password = password_input;

  // Don't inline autocomplete if the caret is not at the end.
  // TODO(jcivelli): is there a better way to test the caret location?
  if (username.selectionStart() != username.selectionEnd() ||
      username.selectionEnd() != static_cast<int>(username.value().length())) {
    return;
  }

  // Show the popup with the list of available usernames.
  ShowSuggestionPopup(fill_data, username, false, false);

#if !defined(OS_ANDROID)
  // Fill the user and password field with the most relevant match. Android
  // only fills in the fields after the user clicks on the suggestion popup.
  if (FillUserNameAndPassword(
          &username,
          &password,
          fill_data,
          false /* exact_username_match */,
          true /* set selection */,
          nonscript_modified_values_,
          base::Bind(&PasswordValueGatekeeper::RegisterElement,
                     base::Unretained(&gatekeeper_)))) {
    usernames_usage_ = OTHER_POSSIBLE_USERNAME_SELECTED;
  }
#endif
}

void PasswordAutofillAgent::FrameClosing() {
  for (auto const& iter : login_to_password_info_) {
    login_to_password_info_key_.erase(iter.first);
    password_to_username_.erase(iter.second.password_field);
  }
  login_to_password_info_.clear();
  provisionally_saved_form_.reset();
  nonscript_modified_values_.clear();
}

bool PasswordAutofillAgent::FindLoginInfo(const blink::WebNode& node,
                                          blink::WebInputElement* found_input,
                                          PasswordInfo** found_password) {
  if (!node.isElementNode())
    return false;

  blink::WebElement element = node.toConst<blink::WebElement>();
  if (!element.hasHTMLTagName("input"))
    return false;

  *found_input = element.to<blink::WebInputElement>();
  const blink::WebInputElement* username_element;  // ignored
  return FindPasswordInfoForElement(*found_input, &username_element,
                                    found_password);
}

void PasswordAutofillAgent::ClearPreview(
    blink::WebInputElement* username,
    blink::WebInputElement* password) {
  if (!username->suggestedValue().isEmpty()) {
    username->setSuggestedValue(blink::WebString());
    username->setAutofilled(was_username_autofilled_);
    username->setSelectionRange(username_selection_start_,
                                username->value().length());
  }
  if (!password->suggestedValue().isEmpty()) {
      password->setSuggestedValue(blink::WebString());
      password->setAutofilled(was_password_autofilled_);
  }
}

void PasswordAutofillAgent::ProvisionallySavePassword(
    const blink::WebFormElement& form,
    ProvisionallySaveRestriction restriction) {
  scoped_ptr<PasswordForm> password_form(
      CreatePasswordForm(form, &nonscript_modified_values_));
  if (!password_form || (restriction == RESTRICTION_NON_EMPTY_PASSWORD &&
                         password_form->password_value.empty() &&
                         password_form->new_password_value.empty())) {
    return;
  }
  provisionally_saved_form_ = password_form.Pass();
}

bool PasswordAutofillAgent::ProvisionallySavedPasswordIsValid() {
   return provisionally_saved_form_ &&
       !provisionally_saved_form_->username_value.empty() &&
       !(provisionally_saved_form_->password_value.empty() &&
         provisionally_saved_form_->new_password_value.empty());
}

// LegacyPasswordAutofillAgent -------------------------------------------------

PasswordAutofillAgent::LegacyPasswordAutofillAgent::LegacyPasswordAutofillAgent(
    content::RenderView* render_view,
    PasswordAutofillAgent* agent)
    : content::RenderViewObserver(render_view), agent_(agent) {
}

PasswordAutofillAgent::LegacyPasswordAutofillAgent::
    ~LegacyPasswordAutofillAgent() {
}

void PasswordAutofillAgent::LegacyPasswordAutofillAgent::OnDestruct() {
  // No op. Do not delete |this|.
}

void PasswordAutofillAgent::LegacyPasswordAutofillAgent::DidStartLoading() {
  agent_->DidStartLoading();
}

void PasswordAutofillAgent::LegacyPasswordAutofillAgent::DidStopLoading() {
  agent_->DidStopLoading();
}

void PasswordAutofillAgent::LegacyPasswordAutofillAgent::
    DidStartProvisionalLoad(blink::WebLocalFrame* navigated_frame) {
  agent_->LegacyDidStartProvisionalLoad(navigated_frame);
}

}  // namespace autofill
