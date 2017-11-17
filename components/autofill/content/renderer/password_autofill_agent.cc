// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_autofill_agent.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/security_state/core/security_state.h"
#include "content/public/common/origin_util.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAutofillClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/modules/password_manager/WebFormElementObserver.h"
#include "third_party/WebKit/public/web/modules/password_manager/WebFormElementObserverCallback.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

namespace autofill {
namespace {

// The size above which we stop triggering autocomplete.
static const size_t kMaximumTextSizeForAutocomplete = 1000;

const char kDummyUsernameField[] = "anonymous_username";
const char kDummyPasswordField[] = "anonymous_password";

// Maps element names to the actual elements to simplify form filling.
typedef std::map<base::string16, blink::WebInputElement> FormInputElementMap;

// Use the shorter name when referencing SavePasswordProgressLogger::StringID
// values to spare line breaks. The code provides enough context for that
// already.
typedef SavePasswordProgressLogger Logger;

typedef std::vector<FormInputElementMap> FormElementsList;

bool FillDataContainsFillableUsername(const PasswordFormFillData& fill_data) {
  return !fill_data.username_field.name.empty() &&
         (!fill_data.additional_logins.empty() ||
          !fill_data.username_field.value.empty());
}

// Returns true if password form has username and password fields with either
// same or no name and id attributes supplied.
bool DoesFormContainAmbiguousOrEmptyNames(
    const PasswordFormFillData& fill_data) {
  return (fill_data.username_field.name == fill_data.password_field.name) ||
         (fill_data.password_field.name ==
              base::ASCIIToUTF16(kDummyPasswordField) &&
          (!FillDataContainsFillableUsername(fill_data) ||
           fill_data.username_field.name ==
               base::ASCIIToUTF16(kDummyUsernameField)));
}

bool IsPasswordField(const FormFieldData& field) {
  return (field.form_control_type == "password");
}

// Returns true if any password field within |control_elements| is supplied with
// either |autocomplete='current-password'| or |autocomplete='new-password'|
// attribute.
bool HasPasswordWithAutocompleteAttribute(
    const std::vector<blink::WebFormControlElement>& control_elements) {
  for (const blink::WebFormControlElement& control_element : control_elements) {
    if (!control_element.HasHTMLTagName("input"))
      continue;

    const blink::WebInputElement input_element =
        control_element.ToConst<blink::WebInputElement>();
    if (input_element.IsPasswordField() &&
        (HasAutocompleteAttributeValue(input_element, "current-password") ||
         HasAutocompleteAttributeValue(input_element, "new-password")))
      return true;
  }

  return false;
}

// Returns the |field|'s autofillable name. If |ambiguous_or_empty_names| is set
// to true returns a dummy name instead.
base::string16 FieldName(const FormFieldData& field,
                         bool ambiguous_or_empty_names) {
  return ambiguous_or_empty_names
             ? IsPasswordField(field) ? base::ASCIIToUTF16(kDummyPasswordField)
                                      : base::ASCIIToUTF16(kDummyUsernameField)
             : field.name;
}

bool IsUnownedPasswordFormVisible(const blink::WebInputElement& input_element) {
  return !input_element.IsNull() &&
         form_util::IsWebElementVisible(input_element);
}

// Utility function to find the unique entry of |control_elements| for the
// specified input |field|. On successful find, adds it to |result| and returns
// |true|. Otherwise clears the references from each |HTMLInputElement| from
// |result| and returns |false|.
bool FindFormInputElement(
    const std::vector<blink::WebFormControlElement>& control_elements,
    const FormFieldData& field,
    bool ambiguous_or_empty_names,
    FormInputElementMap* result) {
  // Match the first input element, if any.
  bool found_input = false;
  bool is_password_field = IsPasswordField(field);
  bool does_password_field_has_ambigous_or_empty_name =
      ambiguous_or_empty_names && is_password_field;
  bool ambiguous_and_multiple_password_fields_with_autocomplete =
      does_password_field_has_ambigous_or_empty_name &&
      HasPasswordWithAutocompleteAttribute(control_elements);
  base::string16 field_name = FieldName(field, ambiguous_or_empty_names);
  for (const blink::WebFormControlElement& control_element : control_elements) {
    if (!ambiguous_or_empty_names &&
        control_element.NameForAutofill().Utf16() != field_name) {
      continue;
    }

    if (!control_element.HasHTMLTagName("input"))
      continue;

    // Only fill saved passwords into password fields and usernames into text
    // fields.
    const blink::WebInputElement input_element =
        control_element.ToConst<blink::WebInputElement>();
    if (!input_element.IsTextField() ||
        input_element.IsPasswordField() != is_password_field)
      continue;

    // For change password form with ambiguous or empty names keep only the
    // first password field having |autocomplete='current-password'| attribute
    // set. Also make sure we avoid keeping password fields having
    // |autocomplete='new-password'| attribute set.
    if (ambiguous_and_multiple_password_fields_with_autocomplete &&
        !HasAutocompleteAttributeValue(input_element, "current-password")) {
      continue;
    }

    // Check for a non-unique match.
    if (found_input) {
      // For change password form keep only the first password field entry.
      if (does_password_field_has_ambigous_or_empty_name) {
        if (!form_util::IsWebElementVisible((*result)[field_name])) {
          // If a previously chosen field was invisible then take the current
          // one.
          (*result)[field_name] = input_element;
        }
        continue;
      }

      found_input = false;
      break;
    }

    (*result)[field_name] = input_element;
    found_input = true;
  }

  // A required element was not found. This is not the right form.
  // Make sure no input elements from a partially matched form in this
  // iteration remain in the result set.
  // Note: clear will remove a reference from each InputElement.
  if (!found_input) {
    result->clear();
    return false;
  }

  return true;
}

// Helper to search through |control_elements| for the specified input elements
// in |data|, and add results to |result|.
bool FindFormInputElements(
    const std::vector<blink::WebFormControlElement>& control_elements,
    const PasswordFormFillData& data,
    bool ambiguous_or_empty_names,
    FormInputElementMap* result) {
  return FindFormInputElement(control_elements, data.password_field,
                              ambiguous_or_empty_names, result) &&
         (!FillDataContainsFillableUsername(data) ||
          FindFormInputElement(control_elements, data.username_field,
                               ambiguous_or_empty_names, result));
}

// Helper to locate form elements identified by |data|.
void FindFormElements(content::RenderFrame* render_frame,
                      const PasswordFormFillData& data,
                      bool ambiguous_or_empty_names,
                      FormElementsList* results) {
  DCHECK(results);

  blink::WebDocument doc = render_frame->GetWebFrame()->GetDocument();

  if (GetSignOnRealm(data.origin) !=
      GetSignOnRealm(form_util::GetCanonicalOriginForDocument(doc)))
    return;

  blink::WebVector<blink::WebFormElement> forms;
  doc.Forms(forms);

  for (size_t i = 0; i < forms.size(); ++i) {
    blink::WebFormElement fe = forms[i];

    // Action URL must match.
    if (data.action != form_util::GetCanonicalActionForForm(fe))
      continue;

    std::vector<blink::WebFormControlElement> control_elements =
        form_util::ExtractAutofillableElementsInForm(fe);
    FormInputElementMap cur_map;
    if (FindFormInputElements(control_elements, data, ambiguous_or_empty_names,
                              &cur_map))
      results->push_back(cur_map);
  }
  // If the element to be filled are not in a <form> element, the "action" and
  // origin should be the same.
  if (data.action != data.origin)
    return;

  std::vector<blink::WebFormControlElement> control_elements =
      form_util::GetUnownedAutofillableFormFieldElements(doc.All(), nullptr);
  FormInputElementMap unowned_elements_map;
  if (FindFormInputElements(control_elements, data, ambiguous_or_empty_names,
                            &unowned_elements_map))
    results->push_back(unowned_elements_map);
}

bool IsElementEditable(const blink::WebInputElement& element) {
  return element.IsEnabled() && !element.IsReadOnly();
}

bool DoUsernamesMatch(const base::string16& potential_suggestion,
                      const base::string16& current_username,
                      bool exact_match) {
  if (potential_suggestion == current_username)
    return true;
  return !exact_match && IsPrefixOfEmailEndingWithAtSign(current_username,
                                                         potential_suggestion);
}

// Returns whether the given |element| is editable.
bool IsElementAutocompletable(const blink::WebInputElement& element) {
  return IsElementEditable(element);
}

// Returns whether the |username_element| is allowed to be autofilled.
//
// Note that if the user interacts with the |password_field| and the
// |username_element| is user-defined (i.e., non-empty and non-autofilled), then
// this function returns false. This is a precaution, to not override the field
// if it has been classified as username by accident.
bool IsUsernameAmendable(const blink::WebInputElement& username_element,
                         bool is_password_field_selected) {
  return !username_element.IsNull() &&
         IsElementAutocompletable(username_element) &&
         (!is_password_field_selected || username_element.IsAutofilled() ||
          username_element.Value().IsEmpty());
}

// Return true if either password_value or new_password_value is not empty and
// not default.
bool FormContainsNonDefaultPasswordValue(const PasswordForm& password_form) {
  return (!password_form.password_value.empty() &&
          !password_form.password_value_is_default) ||
      (!password_form.new_password_value.empty() &&
       !password_form.new_password_value_is_default);
}

// Log a message including the name, method and action of |form|.
void LogHTMLForm(SavePasswordProgressLogger* logger,
                 SavePasswordProgressLogger::StringID message_id,
                 const blink::WebFormElement& form) {
  logger->LogHTMLForm(message_id, form.GetName().Utf8(),
                      GURL(form.Action().Utf8()));
}


// Returns true if there are any suggestions to be derived from |fill_data|.
// Unless |show_all| is true, only considers suggestions with usernames having
// |current_username| as a prefix.
bool CanShowSuggestion(const PasswordFormFillData& fill_data,
                       const base::string16& current_username,
                       bool show_all) {
  base::string16 current_username_lower = base::i18n::ToLower(current_username);
  for (const auto& usernames : fill_data.other_possible_usernames) {
    for (size_t i = 0; i < usernames.second.size(); ++i) {
      if (show_all ||
          base::StartsWith(
              base::i18n::ToLower(base::string16(usernames.second[i])),
              current_username_lower, base::CompareCase::SENSITIVE)) {
        return true;
      }
    }
  }

  if (show_all ||
      base::StartsWith(base::i18n::ToLower(fill_data.username_field.value),
                       current_username_lower, base::CompareCase::SENSITIVE)) {
    return true;
  }

  for (const auto& login : fill_data.additional_logins) {
    if (show_all ||
        base::StartsWith(base::i18n::ToLower(login.first),
                         current_username_lower,
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  return false;
}

// Updates the value (i.e. the pair of elements's value |value| and field
// properties |added_flags|) associated with the key |element| in
// |field_value_and_properties_map|.
// Flags in |added_flags| are added with bitwise OR operation.
// If |value| is null, the value is neither updated nor added.
// If |*value| is empty, USER_TYPED and AUTOFILLED should be cleared.
void UpdateFieldValueAndPropertiesMaskMap(
    const blink::WebFormControlElement& element,
    const base::string16* value,
    FieldPropertiesMask added_flags,
    FieldValueAndPropertiesMaskMap* field_value_and_properties_map) {
  FieldValueAndPropertiesMaskMap::iterator it =
      field_value_and_properties_map->find(element);
  if (it != field_value_and_properties_map->end()) {
    if (value)
      it->second.first.reset(new base::string16(*value));
    it->second.second |= added_flags;
  } else {
    (*field_value_and_properties_map)[element] = std::make_pair(
        value ? base::MakeUnique<base::string16>(*value) : nullptr,
        added_flags);
  }
  // Reset USER_TYPED and AUTOFILLED flags if the value is empty.
  if (value && value->empty()) {
    (*field_value_and_properties_map)[element].second &=
        ~(FieldPropertiesFlags::USER_TYPED | FieldPropertiesFlags::AUTOFILLED);
  }
}

// This function attempts to find the matching credentials for the
// |current_username| by scanning |fill_data|. The result is written in
// |username| and |password| parameters.
void FindMatchesByUsername(const PasswordFormFillData& fill_data,
                           const base::string16& current_username,
                           bool exact_username_match,
                           RendererSavePasswordProgressLogger* logger,
                           base::string16* username,
                           base::string16* password) {
  // Look for any suitable matches to current field text.
  if (DoUsernamesMatch(fill_data.username_field.value, current_username,
                       exact_username_match)) {
    *username = fill_data.username_field.value;
    *password = fill_data.password_field.value;
    if (logger)
      logger->LogMessage(Logger::STRING_USERNAMES_MATCH);
  } else {
    // Scan additional logins for a match.
    for (const auto& it : fill_data.additional_logins) {
      if (DoUsernamesMatch(it.first, current_username, exact_username_match)) {
        *username = it.first;
        *password = it.second.password;
        break;
      }
    }
    if (logger) {
      logger->LogBoolean(Logger::STRING_MATCH_IN_ADDITIONAL,
                         !(username->empty() && password->empty()));
    }

    // Check possible usernames.
    if (username->empty() && password->empty()) {
      for (const auto& it : fill_data.other_possible_usernames) {
        for (size_t i = 0; i < it.second.size(); ++i) {
          if (DoUsernamesMatch(it.second[i], current_username,
                               exact_username_match)) {
            *username = it.second[i];
            *password = it.first.password;
            break;
          }
        }
        if (!password->empty())
          break;
      }
    }
  }
}

// TODO(crbug.com/564578): This duplicates code from
// components/password_manager/core/browser/psl_matching_helper.h. The logic
// using this code should ultimately end up in
// components/password_manager/core/browser, at which point it can use the
// original code directly.
std::string GetRegistryControlledDomain(const GURL& signon_realm) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      signon_realm,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// TODO(crbug.com/564578): This duplicates code from
// components/password_manager/core/browser/psl_matching_helper.h. The logic
// using this code should ultimately end up in
// components/password_manager/core/browser, at which point it can use the
// original code directly.
bool IsPublicSuffixDomainMatch(const std::string& url1,
                               const std::string& url2) {
  GURL gurl1(url1);
  GURL gurl2(url2);

  if (!gurl1.is_valid() || !gurl2.is_valid())
    return false;

  if (gurl1 == gurl2)
    return true;

  std::string domain1(GetRegistryControlledDomain(gurl1));
  std::string domain2(GetRegistryControlledDomain(gurl2));

  if (domain1.empty() || domain2.empty())
    return false;

  return gurl1.scheme() == gurl2.scheme() && domain1 == domain2 &&
         gurl1.port() == gurl2.port();
}

// Annotate |forms| with form and field signatures as HTML attributes.
void AnnotateFormsWithSignatures(
    blink::WebVector<blink::WebFormElement> forms) {
  for (blink::WebFormElement form : forms) {
    std::unique_ptr<PasswordForm> password_form(
        CreatePasswordFormFromWebForm(form, nullptr, nullptr, nullptr));
    if (password_form) {
      form.SetAttribute(
          blink::WebString::FromASCII(kDebugAttributeForFormSignature),
          blink::WebString::FromUTF8(base::Uint64ToString(
              CalculateFormSignature(password_form->form_data))));

      std::vector<blink::WebFormControlElement> control_elements =
          form_util::ExtractAutofillableElementsInForm(form);

      if (control_elements.size() != password_form->form_data.fields.size())
        return;

      for (size_t i = 0; i < control_elements.size(); ++i) {
        blink::WebFormControlElement control_element = control_elements[i];

        const FormFieldData& field = password_form->form_data.fields[i];
        if (field.name != control_element.NameForAutofill().Utf16())
          continue;
        control_element.SetAttribute(
            blink::WebString::FromASCII(kDebugAttributeForFieldSignature),
            blink::WebString::FromUTF8(
                base::Uint64ToString(CalculateFieldSignatureForField(field))));
      }
    }
  }
}

// Returns true iff there is a password field in |frame|.
bool HasPasswordField(const blink::WebLocalFrame& frame) {
  CR_DEFINE_STATIC_LOCAL(blink::WebString, kPassword, ("password"));

  const blink::WebElementCollection elements = frame.GetDocument().All();
  for (blink::WebElement element = elements.FirstItem(); !element.IsNull();
       element = elements.NextItem()) {
    if (element.IsFormControlElement()) {
      const blink::WebFormControlElement& control =
          element.To<blink::WebFormControlElement>();
      if (control.FormControlType() == kPassword)
        return true;
    }
  }
  return false;
}

// Returns the closest visible autocompletable non-password text element
// preceding the |password_element| either in a form, if it belongs to one, or
// in the |frame|.
blink::WebInputElement FindUsernameElementPrecedingPasswordElement(
    blink::WebLocalFrame* frame,
    const blink::WebInputElement& password_element) {
  DCHECK(!password_element.IsNull());

  std::vector<blink::WebFormControlElement> elements;
  if (password_element.Form().IsNull()) {
    elements = form_util::GetUnownedAutofillableFormFieldElements(
        frame->GetDocument().All(), nullptr);
  } else {
    blink::WebVector<blink::WebFormControlElement> web_control_elements;
    password_element.Form().GetFormControlElements(web_control_elements);
    elements.assign(web_control_elements.begin(), web_control_elements.end());
  }

  auto iter = std::find(elements.begin(), elements.end(), password_element);
  if (iter == elements.end())
    return blink::WebInputElement();

  for (auto begin = elements.begin(); iter != begin;) {
    --iter;
    const blink::WebInputElement* input = blink::ToWebInputElement(&*iter);
    if (input && input->IsTextField() && !input->IsPasswordField() &&
        IsElementAutocompletable(*input) &&
        form_util::IsWebElementVisible(*input)) {
      return *input;
    }
  }

  return blink::WebInputElement();
}

bool ShouldShowStandaloneManuallFallback(const blink::WebInputElement& element,
                                         const GURL& url) {
  return (
      element.IsPasswordField() &&
      !IsCreditCardVerificationPasswordField(element) &&
      !HasCreditCardAutocompleteAttributes(element) &&
      !base::StartsWith(url.scheme(), "chrome", base::CompareCase::SENSITIVE) &&
      !url.SchemeIs(url::kAboutScheme) &&
      base::FeatureList::IsEnabled(
          password_manager::features::kEnableManualFallbacksFillingStandalone));
}

}  // namespace

class PasswordAutofillAgent::FormElementObserverCallback
    : public blink::WebFormElementObserverCallback {
 public:
  explicit FormElementObserverCallback(PasswordAutofillAgent* agent)
      : agent_(agent) {}
  ~FormElementObserverCallback() override = default;

  void ElementWasHiddenOrRemoved() override {
    agent_->OnSameDocumentNavigationCompleted(
        PasswordForm::SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR);
  }

 private:
  PasswordAutofillAgent* agent_;

  DISALLOW_COPY_AND_ASSIGN(FormElementObserverCallback);
};

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, public:

PasswordAutofillAgent::PasswordAutofillAgent(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      web_input_to_password_info_(),
      last_supplied_password_info_iter_(web_input_to_password_info_.end()),
      logging_state_active_(false),
      was_username_autofilled_(false),
      was_password_autofilled_(false),
      sent_request_to_store_(false),
      checked_safe_browsing_reputation_(false),
      binding_(this),
      form_element_observer_(nullptr) {
  registry->AddInterface(
      base::Bind(&PasswordAutofillAgent::BindRequest, base::Unretained(this)));
}

PasswordAutofillAgent::~PasswordAutofillAgent() {
  if (form_element_observer_) {
    form_element_observer_->Disconnect();
    form_element_observer_ = nullptr;
  }
}

void PasswordAutofillAgent::BindRequest(
    mojom::PasswordAutofillAgentRequest request) {
  binding_.Bind(std::move(request));
}

void PasswordAutofillAgent::SetAutofillAgent(AutofillAgent* autofill_agent) {
  autofill_agent_ = autofill_agent;
}

void PasswordAutofillAgent::SetPasswordGenerationAgent(
    PasswordGenerationAgent* generation_agent) {
  password_generation_agent_ = generation_agent;
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
  if (was_user_gesture_seen_)
    return;

  was_user_gesture_seen_ = true;

  for (blink::WebInputElement& element : elements_)
    ShowValue(&element);

  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::Reset() {
  was_user_gesture_seen_ = false;
  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::ShowValue(
    blink::WebInputElement* element) {
  if (!element->IsNull() && !element->SuggestedValue().IsEmpty())
    element->SetAutofillValue(element->SuggestedValue());
}

bool PasswordAutofillAgent::TextDidChangeInTextField(
    const blink::WebInputElement& element) {
  // TODO(vabr): Get a mutable argument instead. http://crbug.com/397083
  blink::WebInputElement mutable_element = element;  // We need a non-const.
  mutable_element.SetAutofilled(false);

  WebInputToPasswordInfoMap::iterator iter =
      web_input_to_password_info_.find(element);
  if (iter != web_input_to_password_info_.end()) {
    iter->second.password_was_edited_last = false;
  }

  // Show the popup with the list of available usernames.
  return ShowSuggestions(element, false, false);
}

void PasswordAutofillAgent::UpdateStateForTextChange(
    const blink::WebInputElement& element) {
  // TODO(vabr): Get a mutable argument instead. http://crbug.com/397083
  blink::WebInputElement mutable_element = element;  // We need a non-const.

  if (element.IsTextField()) {
    const base::string16 element_value = element.Value().Utf16();
    UpdateFieldValueAndPropertiesMaskMap(element, &element_value,
                                         FieldPropertiesFlags::USER_TYPED,
                                         &field_value_and_properties_map_);
  }

  ProvisionallySavePassword(element.Form(), element, RESTRICTION_NONE);

  if (element.IsPasswordField()) {
    PasswordToLoginMap::iterator iter = password_to_username_.find(element);
    if (iter != password_to_username_.end()) {
      web_input_to_password_info_[iter->second].password_was_edited_last = true;
      // Note that the suggested value of |mutable_element| was reset when its
      // value changed.
      mutable_element.SetAutofilled(false);
    }
  }

  if (element.IsPasswordField())
    GetPasswordManagerDriver()->UserModifiedPasswordField();
}

bool PasswordAutofillAgent::FillSuggestion(
    const blink::WebFormControlElement& control_element,
    const base::string16& username,
    const base::string16& password) {
  // The element in context of the suggestion popup.
  const blink::WebInputElement* element = ToWebInputElement(&control_element);
  if (!element)
    return false;

  blink::WebInputElement username_element;
  blink::WebInputElement password_element;
  PasswordInfo* password_info = nullptr;

  if (!FindPasswordInfoForElement(*element, &username_element,
                                  &password_element, &password_info) ||
      !IsElementAutocompletable(password_element)) {
    return false;
  }

  password_info->password_was_edited_last = false;
  if (element->IsPasswordField()) {
    password_info->password_field_suggestion_was_accepted = true;
    password_info->password_field = password_element;
  }

  // Call OnFieldAutofilled before WebInputElement::SetAutofilled which may
  // cause frame closing.
  if (password_generation_agent_)
    password_generation_agent_->OnFieldAutofilled(password_element);

  if (IsUsernameAmendable(username_element, element->IsPasswordField()) &&
      username_element.Value().Utf16() != username) {
    username_element.SetAutofillValue(blink::WebString::FromUTF16(username));
    username_element.SetAutofilled(true);
    UpdateFieldValueAndPropertiesMaskMap(username_element, &username,
                                         FieldPropertiesFlags::AUTOFILLED,
                                         &field_value_and_properties_map_);
  }

  password_element.SetAutofillValue(blink::WebString::FromUTF16(password));
  password_element.SetAutofilled(true);
  UpdateFieldValueAndPropertiesMaskMap(password_element, &password,
                                       FieldPropertiesFlags::AUTOFILLED,
                                       &field_value_and_properties_map_);
  ProvisionallySavePassword(password_element.Form(), password_element,
                            RESTRICTION_NONE);

  blink::WebInputElement mutable_filled_element = *element;
  mutable_filled_element.SetSelectionRange(element->Value().length(),
                                           element->Value().length());

  return true;
}

bool PasswordAutofillAgent::PreviewSuggestion(
    const blink::WebFormControlElement& control_element,
    const blink::WebString& username,
    const blink::WebString& password) {
  // The element in context of the suggestion popup.
  const blink::WebInputElement* element = ToWebInputElement(&control_element);
  if (!element)
    return false;

  blink::WebInputElement username_element;
  blink::WebInputElement password_element;
  PasswordInfo* password_info;

  if (!FindPasswordInfoForElement(*element, &username_element,
                                  &password_element, &password_info) ||
      !IsElementAutocompletable(password_element)) {
    return false;
  }

  if (IsUsernameAmendable(username_element, element->IsPasswordField())) {
    if (username_query_prefix_.empty())
      username_query_prefix_ = username_element.Value().Utf16();

    was_username_autofilled_ = username_element.IsAutofilled();
    username_element.SetSuggestedValue(username);
    username_element.SetAutofilled(true);
    form_util::PreviewSuggestion(username_element.SuggestedValue().Utf16(),
                                 username_query_prefix_, &username_element);
  }
  was_password_autofilled_ = password_element.IsAutofilled();
  password_element.SetSuggestedValue(password);
  password_element.SetAutofilled(true);

  return true;
}

bool PasswordAutofillAgent::DidClearAutofillSelection(
    const blink::WebFormControlElement& control_element) {
  const blink::WebInputElement* element = ToWebInputElement(&control_element);
  if (!element)
    return false;

  blink::WebInputElement username_element;
  blink::WebInputElement password_element;
  PasswordInfo* password_info;

  if (!FindPasswordInfoForElement(*element, &username_element,
                                  &password_element, &password_info)) {
    return false;
  }

  ClearPreview(&username_element, &password_element);
  return true;
}

bool PasswordAutofillAgent::FindPasswordInfoForElement(
    const blink::WebInputElement& element,
    blink::WebInputElement* username_element,
    blink::WebInputElement* password_element,
    PasswordInfo** password_info) {
  DCHECK(username_element && password_element && password_info);
  username_element->Reset();
  password_element->Reset();
  if (!element.IsPasswordField()) {
    *username_element = element;
  } else {
    // If there is a password field, but a request to the store hasn't been sent
    // yet, then do fetch saved credentials now.
    if (!sent_request_to_store_) {
      SendPasswordForms(false);
      return false;
    }

    *password_element = element;

    WebInputToPasswordInfoMap::iterator iter =
        web_input_to_password_info_.find(element);
    if (iter == web_input_to_password_info_.end()) {
      PasswordToLoginMap::const_iterator password_iter =
          password_to_username_.find(element);
      if (password_iter == password_to_username_.end()) {
        if (web_input_to_password_info_.empty())
          return false;
        iter = last_supplied_password_info_iter_;
      } else {
        *username_element = password_iter->second;
      }
    }

    if (iter != web_input_to_password_info_.end()) {
      // It's a password field without corresponding username field. Try to find
      // the username field based on visibility.
      *username_element = FindUsernameElementPrecedingPasswordElement(
          render_frame()->GetWebFrame(), *password_element);
      *password_info = &iter->second;
      return true;
    }
    // Otherwise |username_element| has been set above.
  }

  WebInputToPasswordInfoMap::iterator iter =
      web_input_to_password_info_.find(*username_element);
  if (iter == web_input_to_password_info_.end())
    return false;

  *password_info = &iter->second;
  if (password_element->IsNull())
    *password_element = (*password_info)->password_field;

  return true;
}

bool PasswordAutofillAgent::ShouldShowNotSecureWarning(
    const blink::WebInputElement& element) {
  // Do not show a warning if the feature is disabled or the context is secure.
  return security_state::IsHttpWarningInFormEnabled() &&
         !content::IsOriginSecure(
             url::Origin(
                 render_frame()->GetWebFrame()->Top()->GetSecurityOrigin())
                 .GetURL());
}

bool PasswordAutofillAgent::IsUsernameOrPasswordField(
    const blink::WebInputElement& element) {
  // Note: A site may use a Password field to collect a CVV or a Credit Card
  // number, but showing a slightly misleading warning here is better than
  // showing no warning at all.
  if (element.IsPasswordField())
    return true;

  // If a field declares itself a username input, show the warning.
  if (HasAutocompleteAttributeValue(element, "username"))
    return true;

  // Otherwise, analyze the form and return true if this input element seems
  // to be the username field.
  std::unique_ptr<PasswordForm> password_form;
  if (element.Form().IsNull()) {
    // To double check that element's frame and |render_frame()->GetWebFrame()|
    // which is used in |GetPasswordFormFromUnownedInputElements| are identical.
    DCHECK_EQ(element.GetDocument().GetFrame(), render_frame()->GetWebFrame());
    password_form = GetPasswordFormFromUnownedInputElements();
  } else {
    password_form = GetPasswordFormFromWebForm(element.Form());
  }

  if (!password_form)
    return false;
  return (password_form->username_element == element.NameForAutofill().Utf16());
}

bool PasswordAutofillAgent::ShowSuggestions(
    const blink::WebInputElement& element,
    bool show_all,
    bool generation_popup_showing) {
  blink::WebInputElement username_element;
  blink::WebInputElement password_element;
  PasswordInfo* password_info;

  if (!FindPasswordInfoForElement(element, &username_element, &password_element,
                                  &password_info)) {
    if (IsUsernameOrPasswordField(element)) {
      blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
      GURL frame_url = GURL(frame->GetDocument().Url());
#if defined(SAFE_BROWSING_DB_LOCAL)
      if (!checked_safe_browsing_reputation_) {
        checked_safe_browsing_reputation_ = true;
        GURL action_url =
            element.Form().IsNull()
                ? GURL()
                : form_util::GetCanonicalActionForForm(element.Form());
        GetPasswordManagerDriver()->CheckSafeBrowsingReputation(action_url,
                                                                frame_url);
      }
#endif
      if (!generation_popup_showing && !blacklisted_form_found_ &&
          ShouldShowStandaloneManuallFallback(element, frame_url) &&
          ShowManualFallbackSuggestion(element)) {
        return true;
      }

      if (ShouldShowNotSecureWarning(element)) {
        autofill_agent_->ShowNotSecureWarning(element);
        return true;
      }
    }
    return false;
  }

  // If autocomplete='off' is set on the form elements, no suggestion dialog
  // should be shown. However, return |true| to indicate that this is a known
  // password form and that the request to show suggestions has been handled (as
  // a no-op).
  if (!element.IsTextField() || !IsElementAutocompletable(element) ||
      !IsElementAutocompletable(password_element))
    return true;

  if (element.NameForAutofill().IsEmpty() &&
      !DoesFormContainAmbiguousOrEmptyNames(password_info->fill_data)) {
    return false;  // If the field has no name, then we won't have values.
  }

  // Don't attempt to autofill with values that are too large.
  if (element.Value().length() > kMaximumTextSizeForAutocomplete)
    return false;

  // If the element is a password field, do not to show a popup if the user has
  // already accepted a password suggestion on another password field.
  if (element.IsPasswordField() &&
      (password_info->password_field_suggestion_was_accepted &&
       element != password_info->password_field))
    return true;

  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.AutocompletePopupSuppressedByGeneration",
      generation_popup_showing);

  if (generation_popup_showing)
    return false;

  // Chrome should never show more than one account for a password element since
  // this implies that the username element cannot be modified. Thus even if
  // |show_all| is true, check if the element in question is a password element
  // for the call to ShowSuggestionPopup.
  return ShowSuggestionPopup(*password_info, element,
                             show_all && !element.IsPasswordField(),
                             element.IsPasswordField());
}

void PasswordAutofillAgent::ShowNotSecureWarning(
    const blink::WebInputElement& element) {
  FormData form;
  FormFieldData field;
  form_util::FindFormAndFieldForFormControlElement(element, &form, &field);
  GetPasswordManagerDriver()->ShowNotSecureWarning(
      field.text_direction,
      render_frame()->GetRenderView()->ElementBoundsInWindow(element));
}

bool PasswordAutofillAgent::FrameCanAccessPasswordManager() {
  // about:blank or about:srcdoc frames should not be allowed to use password
  // manager.  See https://crbug.com/756587.
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (frame->GetDocument().Url().ProtocolIs(url::kAboutScheme))
    return false;
  return frame->GetSecurityOrigin().CanAccessPasswordManager();
}

void PasswordAutofillAgent::OnDynamicFormsSeen() {
  SendPasswordForms(false /* only_visible */);
}

void PasswordAutofillAgent::AJAXSucceeded() {
  OnSameDocumentNavigationCompleted(
      PasswordForm::SubmissionIndicatorEvent::XHR_SUCCEEDED);
}

void PasswordAutofillAgent::OnSameDocumentNavigationCompleted(
    PasswordForm::SubmissionIndicatorEvent event) {
  if (!provisionally_saved_form_.IsPasswordValid())
    return;

  DCHECK(FrameCanAccessPasswordManager());

  // Prompt to save only if the form is now gone, either invisible or
  // removed from the DOM.
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  const auto& password_form = provisionally_saved_form_.password_form();
  // TODO(crbug.com/720347): This method could be called often and checking form
  // visibility could be expesive. Add performance metrics for this.
  if (event != PasswordForm::SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR) {
    if (form_util::IsFormVisible(frame,
                                 provisionally_saved_form_.form_element(),
                                 password_form.action, password_form.origin,
                                 password_form.form_data) ||
        (provisionally_saved_form_.form_element().IsNull() &&
         IsUnownedPasswordFormVisible(
             provisionally_saved_form_.input_element()))) {
      if (!form_element_observer_) {
        std::unique_ptr<FormElementObserverCallback> callback(
            new FormElementObserverCallback(this));
        if (!provisionally_saved_form_.form_element().IsNull()) {
          form_element_observer_ = blink::WebFormElementObserver::Create(
              provisionally_saved_form_.form_element(), std::move(callback));
        } else if (!provisionally_saved_form_.input_element().IsNull()) {
          form_element_observer_ = blink::WebFormElementObserver::Create(
              provisionally_saved_form_.input_element(), std::move(callback));
        }
      }
      return;
    }
  }

  provisionally_saved_form_.SetSubmissionIndicatorEvent(event);
  GetPasswordManagerDriver()->InPageNavigation(password_form);
  if (form_element_observer_) {
    form_element_observer_->Disconnect();
    form_element_observer_ = nullptr;
  }
  provisionally_saved_form_.Reset();
}

void PasswordAutofillAgent::UserGestureObserved() {
  gatekeeper_.OnUserGesture();
}

void PasswordAutofillAgent::SendPasswordForms(bool only_visible) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_SEND_PASSWORD_FORMS_METHOD);
    logger->LogBoolean(Logger::STRING_ONLY_VISIBLE, only_visible);
  }

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();

  // Make sure that this security origin is allowed to use password manager.
  blink::WebSecurityOrigin origin = frame->GetDocument().GetSecurityOrigin();
  if (logger) {
    logger->LogURL(Logger::STRING_SECURITY_ORIGIN,
                   GURL(origin.ToString().Utf8()));
  }
  if (!FrameCanAccessPasswordManager()) {
    if (logger)
      logger->LogMessage(Logger::STRING_SECURITY_ORIGIN_FAILURE);
    return;
  }

  // Checks whether the webpage is a redirect page or an empty page.
  if (form_util::IsWebpageEmpty(frame)) {
    if (logger) {
      logger->LogMessage(Logger::STRING_WEBPAGE_EMPTY);
    }
    return;
  }

  blink::WebVector<blink::WebFormElement> forms;
  frame->GetDocument().Forms(forms);

  if (IsShowAutofillSignaturesEnabled())
    AnnotateFormsWithSignatures(forms);
  if (logger)
    logger->LogNumber(Logger::STRING_NUMBER_OF_ALL_FORMS, forms.size());

  std::vector<PasswordForm> password_forms;
  for (const blink::WebFormElement& form : forms) {
    if (IsGaiaReauthenticationForm(form)) {
      // Bail if this is a GAIA passwords site reauthentication form, so that
      // page will be ignored.
      return;
    }
    if (only_visible) {
      bool is_form_visible = form_util::AreFormContentsVisible(form);
      if (logger) {
        LogHTMLForm(logger.get(), Logger::STRING_FORM_FOUND_ON_PAGE, form);
        logger->LogBoolean(Logger::STRING_FORM_IS_VISIBLE, is_form_visible);
      }

      // If requested, ignore non-rendered forms, e.g., those styled with
      // display:none.
      if (!is_form_visible)
        continue;
    }

    std::unique_ptr<PasswordForm> password_form(
        GetPasswordFormFromWebForm(form));
    if (password_form) {
      if (logger) {
        logger->LogPasswordForm(Logger::STRING_FORM_IS_PASSWORD,
                                *password_form);
      }
      password_forms.push_back(*password_form);
    }
  }

  // See if there are any unattached input elements that could be used for
  // password submission.
  bool add_unowned_inputs = true;
  if (only_visible) {
    std::vector<blink::WebFormControlElement> control_elements =
        form_util::GetUnownedAutofillableFormFieldElements(
            frame->GetDocument().All(), nullptr);
    add_unowned_inputs =
        form_util::IsSomeControlElementVisible(control_elements);
    if (logger) {
      logger->LogBoolean(Logger::STRING_UNOWNED_INPUTS_VISIBLE,
                         add_unowned_inputs);
    }
  }
  if (add_unowned_inputs) {
    std::unique_ptr<PasswordForm> password_form(
        GetPasswordFormFromUnownedInputElements());
    if (password_form) {
      if (logger) {
        logger->LogPasswordForm(Logger::STRING_FORM_IS_PASSWORD,
                                *password_form);
      }
      password_forms.push_back(*password_form);
    }
  }

  if (only_visible) {
    // Send the PasswordFormsRendered message regardless of whether
    // |password_forms| is empty. The empty |password_forms| are a possible
    // signal to the browser that a pending login attempt succeeded.
    blink::WebFrame* main_frame = render_frame()->GetWebFrame()->Top();
    bool did_stop_loading = !main_frame || !main_frame->IsLoading();
    GetPasswordManagerDriver()->PasswordFormsRendered(password_forms,
                                                      did_stop_loading);
  } else {
    // If there is a password field, but the list of password forms is empty for
    // some reason, add a dummy form to the list. It will cause a request to the
    // store. Therefore, saved passwords will be available for filling on click.
    if (!sent_request_to_store_ && password_forms.empty() &&
        HasPasswordField(*frame)) {
      // Set everything that |FormDigest| needs.
      password_forms.push_back(PasswordForm());
      password_forms.back().scheme = PasswordForm::SCHEME_HTML;
      password_forms.back().origin =
          form_util::GetCanonicalOriginForDocument(frame->GetDocument());
      password_forms.back().signon_realm =
          GetSignOnRealm(password_forms.back().origin);
    }
    if (!password_forms.empty()) {
      sent_request_to_store_ = true;
      GetPasswordManagerDriver()->PasswordFormsParsed(password_forms);
    }
  }

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Provide warnings about the accessibility of password forms on the page.
  if (!password_forms.empty() &&
      (frame->GetDocument().Url().ProtocolIs(url::kHttpScheme) ||
       frame->GetDocument().Url().ProtocolIs(url::kHttpsScheme)))
    page_passwords_analyser_.AnalyseDocumentDOM(frame);
#endif
}

void PasswordAutofillAgent::DidFinishDocumentLoad() {
  // The |frame| contents have been parsed, but not yet rendered.  Let the
  // PasswordManager know that forms are loaded, even though we can't yet tell
  // whether they're visible.
  form_util::ScopedLayoutPreventer layout_preventer;
  SendPasswordForms(false);
}

void PasswordAutofillAgent::DidFinishLoad() {
  // The |frame| contents have been rendered.  Let the PasswordManager know
  // which of the loaded frames are actually visible to the user.  This also
  // triggers the "Save password?" infobar if the user just submitted a password
  // form.
  SendPasswordForms(true);
}

void PasswordAutofillAgent::WillCommitProvisionalLoad() {
  FrameClosing();
}

void PasswordAutofillAgent::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_document_navigation) {
  if (is_same_document_navigation) {
    OnSameDocumentNavigationCompleted(
        PasswordForm::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  } else {
    checked_safe_browsing_reputation_ = false;
  }
}

void PasswordAutofillAgent::FrameDetached() {
  // If a sub frame has been destroyed while the user was entering information
  // into a password form, try to save the data. See https://crbug.com/450806
  // for examples of sites that perform login using this technique.
  if (render_frame()->GetWebFrame()->Parent() &&
      provisionally_saved_form_.IsPasswordValid()) {
    DCHECK(FrameCanAccessPasswordManager());
    provisionally_saved_form_.SetSubmissionIndicatorEvent(
        PasswordForm::SubmissionIndicatorEvent::FRAME_DETACHED);
    GetPasswordManagerDriver()->InPageNavigation(
        provisionally_saved_form_.password_form());
  }
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
  ProvisionallySavePassword(form, blink::WebInputElement(),
                            RESTRICTION_NON_EMPTY_PASSWORD);
}

void PasswordAutofillAgent::WillSubmitForm(const blink::WebFormElement& form) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_WILL_SUBMIT_FORM_METHOD);
    LogHTMLForm(logger.get(), Logger::STRING_HTML_FORM_FOR_SUBMIT, form);
  }

  std::unique_ptr<PasswordForm> submitted_form =
      GetPasswordFormFromWebForm(form);

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
    if (provisionally_saved_form_.IsSet() &&
        submitted_form->action ==
            provisionally_saved_form_.password_form().action) {
      if (logger)
        logger->LogMessage(Logger::STRING_SUBMITTED_PASSWORD_REPLACED);
      const auto& saved_form = provisionally_saved_form_.password_form();
      submitted_form->password_value = saved_form.password_value;
      submitted_form->new_password_value = saved_form.new_password_value;
      submitted_form->username_value = saved_form.username_value;
      submitted_form->submission_event =
          PasswordForm::SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
    }

    if (FrameCanAccessPasswordManager()) {
      // Some observers depend on sending this information now instead of when
      // the frame starts loading. If there are redirects that cause a new
      // RenderView to be instantiated (such as redirects to the WebStore)
      // we will never get to finish the load.
      GetPasswordManagerDriver()->PasswordFormSubmitted(*submitted_form);
    } else {
      if (logger)
        logger->LogMessage(Logger::STRING_SECURITY_ORIGIN_FAILURE);

      // Record how often users submit passwords on about:blank frames.
      if (form.GetDocument().Url().ProtocolIs(url::kAboutScheme)) {
        bool is_main_frame = !form.GetDocument().GetFrame()->Parent();
        UMA_HISTOGRAM_BOOLEAN("PasswordManager.AboutBlankPasswordSubmission",
                              is_main_frame);
      }
    }

    if (form_element_observer_) {
      form_element_observer_->Disconnect();
      form_element_observer_ = nullptr;
    }
    provisionally_saved_form_.Reset();
  } else if (logger) {
    logger->LogMessage(Logger::STRING_FORM_IS_NOT_PASSWORD);
  }
}

void PasswordAutofillAgent::OnDestruct() {
  binding_.Close();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void PasswordAutofillAgent::DidStartProvisionalLoad(
    blink::WebDocumentLoader* document_loader) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_DID_START_PROVISIONAL_LOAD_METHOD);
  }

  blink::WebLocalFrame* navigated_frame = render_frame()->GetWebFrame();
  if (navigated_frame->Parent()) {
    if (logger)
      logger->LogMessage(Logger::STRING_FRAME_NOT_MAIN_FRAME);
    return;
  }

  // This is a new navigation, so require a new user gesture before filling in
  // passwords.
  gatekeeper_.Reset();

  if (!FrameCanAccessPasswordManager()) {
    if (logger)
      logger->LogMessage(Logger::STRING_SECURITY_ORIGIN_FAILURE);
    return;
  }

  // Bug fix for crbug.com/368690. isProcessingUserGesture() is false when
  // the user is performing actions outside the page (e.g. typed url,
  // history navigation). We don't want to trigger saving in these cases.
  content::DocumentState* document_state =
      content::DocumentState::FromDocumentLoader(document_loader);
  content::NavigationState* navigation_state =
      document_state->navigation_state();
  ui::PageTransition type = navigation_state->GetTransitionType();
  if (ui::PageTransitionIsWebTriggerable(type) &&
      ui::PageTransitionIsNewNavigation(type) &&
      !blink::WebUserGestureIndicator::IsProcessingUserGesture(
          navigated_frame)) {
    // If onsubmit has been called, try and save that form.
    if (provisionally_saved_form_.IsSet()) {
      if (logger) {
        logger->LogPasswordForm(
            Logger::STRING_PROVISIONALLY_SAVED_FORM_FOR_FRAME,
            provisionally_saved_form_.password_form());
      }
      provisionally_saved_form_.SetSubmissionIndicatorEvent(
          PasswordForm::SubmissionIndicatorEvent::
              PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD);
      GetPasswordManagerDriver()->PasswordFormSubmitted(
          provisionally_saved_form_.password_form());
      if (form_element_observer_) {
        form_element_observer_->Disconnect();
        form_element_observer_ = nullptr;
      }
      provisionally_saved_form_.Reset();
    } else {
      std::vector<std::unique_ptr<PasswordForm>> possible_submitted_forms;
      // Loop through the forms on the page looking for one that has been
      // filled out. If one exists, try and save the credentials.
      blink::WebVector<blink::WebFormElement> forms;
      render_frame()->GetWebFrame()->GetDocument().Forms(forms);

      bool password_forms_found = false;
      for (size_t i = 0; i < forms.size(); ++i) {
        blink::WebFormElement form_element = forms[i];
        if (logger) {
          LogHTMLForm(logger.get(), Logger::STRING_FORM_FOUND_ON_PAGE,
                      form_element);
        }
        std::unique_ptr<PasswordForm> form =
            GetPasswordFormFromWebForm(form_element);
        if (form) {
          form->submission_event = PasswordForm::SubmissionIndicatorEvent::
              FILLED_FORM_ON_START_PROVISIONAL_LOAD;
          possible_submitted_forms.push_back(std::move(form));
        }
      }

      std::unique_ptr<PasswordForm> form =
          GetPasswordFormFromUnownedInputElements();
      if (form) {
        form->submission_event = PasswordForm::SubmissionIndicatorEvent::
            FILLED_INPUT_ELEMENTS_ON_START_PROVISIONAL_LOAD;
        possible_submitted_forms.push_back(std::move(form));
      }

      for (const auto& password_form : possible_submitted_forms) {
        if (password_form && !password_form->username_value.empty() &&
            FormContainsNonDefaultPasswordValue(*password_form)) {
          password_forms_found = true;
          if (logger) {
            logger->LogPasswordForm(Logger::STRING_PASSWORD_FORM_FOUND_ON_PAGE,
                                    *password_form);
          }
          GetPasswordManagerDriver()->PasswordFormSubmitted(*password_form);
          break;
        }
      }

      if (!password_forms_found && logger)
        logger->LogMessage(Logger::STRING_PASSWORD_FORM_NOT_FOUND_ON_PAGE);
    }
  }
}

// mojom::PasswordAutofillAgent:
void PasswordAutofillAgent::FillPasswordForm(
    int key,
    const PasswordFormFillData& form_data) {
  std::vector<blink::WebInputElement> elements;
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_ON_FILL_PASSWORD_FORM_METHOD);
  }
  GetFillableElementFromFormData(key, form_data, logger.get(), &elements);

  // If wait_for_username is true, we don't want to initially fill the form
  // until the user types in a valid username.
  if (form_data.wait_for_username)
    return;

  for (auto element : elements) {
    blink::WebInputElement username_element =
        !element.IsPasswordField() ? element : password_to_username_[element];
    blink::WebInputElement password_element =
        element.IsPasswordField()
            ? element
            : web_input_to_password_info_[element].password_field;
    FillFormOnPasswordReceived(
        form_data, username_element, password_element,
        &field_value_and_properties_map_,
        base::Bind(&PasswordValueGatekeeper::RegisterElement,
                   base::Unretained(&gatekeeper_)),
        logger.get());
  }
}

void PasswordAutofillAgent::GetFillableElementFromFormData(
    int key,
    const PasswordFormFillData& form_data,
    RendererSavePasswordProgressLogger* logger,
    std::vector<blink::WebInputElement>* elements) {
  DCHECK(elements);
  bool ambiguous_or_empty_names =
      DoesFormContainAmbiguousOrEmptyNames(form_data);
  FormElementsList forms;
  FindFormElements(render_frame(), form_data, ambiguous_or_empty_names, &forms);
  if (logger) {
    logger->LogBoolean(Logger::STRING_AMBIGUOUS_OR_EMPTY_NAMES,
                       ambiguous_or_empty_names);
    logger->LogNumber(Logger::STRING_NUMBER_OF_POTENTIAL_FORMS_TO_FILL,
                      forms.size());
    logger->LogBoolean(Logger::STRING_FORM_DATA_WAIT,
                       form_data.wait_for_username);
  }
  for (const auto& form : forms) {
    base::string16 username_field_name;
    base::string16 password_field_name =
        FieldName(form_data.password_field, ambiguous_or_empty_names);
    bool form_contains_fillable_username_field =
        FillDataContainsFillableUsername(form_data);
    if (form_contains_fillable_username_field) {
      username_field_name =
          FieldName(form_data.username_field, ambiguous_or_empty_names);
    }
    if (logger) {
      logger->LogBoolean(Logger::STRING_CONTAINS_FILLABLE_USERNAME_FIELD,
                         form_contains_fillable_username_field);
      logger->LogBoolean(Logger::STRING_USERNAME_FIELD_NAME_EMPTY,
                         username_field_name.empty());
      logger->LogBoolean(Logger::STRING_PASSWORD_FIELD_NAME_EMPTY,
                         password_field_name.empty());
    }

    // Attach autocomplete listener to enable selecting alternate logins.
    blink::WebInputElement username_element;
    blink::WebInputElement password_element;

    // Check whether the password form has a username input field.
    if (!username_field_name.empty()) {
      const auto it = form.find(username_field_name);
      DCHECK(it != form.end());
      username_element = it->second;
    }

    // No password field, bail out.
    if (password_field_name.empty())
      break;

    // Get pointer to password element. (We currently only support single
    // password forms).
    {
      const auto it = form.find(password_field_name);
      DCHECK(it != form.end());
      password_element = it->second;
    }

    blink::WebInputElement main_element =
        username_element.IsNull() ? password_element : username_element;

    PasswordInfo password_info;
    password_info.fill_data = form_data;
    password_info.key = key;
    password_info.password_field = password_element;
    web_input_to_password_info_[main_element] = password_info;
    last_supplied_password_info_iter_ =
        web_input_to_password_info_.find(main_element);
    if (!main_element.IsPasswordField())
      password_to_username_[password_element] = username_element;
    if (elements)
      elements->push_back(main_element);
  }

  // This is a fallback, if for some reasons elements for filling were not found
  // (for example because they were renamed by JavaScript) then add fill data
  // for |web_input_to_password_info_|. When the user clicks on a password
  // field which is not a key in |web_input_to_password_info_|, the first
  // element from |web_input_to_password_info_| will be used in
  // PasswordAutofillAgent::FindPasswordInfoForElement to propose to fill.
  if (web_input_to_password_info_.empty()) {
    PasswordInfo password_info;
    password_info.fill_data = form_data;
    password_info.key = key;
    web_input_to_password_info_[blink::WebInputElement()] = password_info;
    last_supplied_password_info_iter_ = web_input_to_password_info_.begin();
  }
}

void PasswordAutofillAgent::FocusedNodeHasChanged(const blink::WebNode& node) {
  if (node.IsNull() || !node.IsElementNode())
    return;
  const blink::WebElement web_element = node.ToConst<blink::WebElement>();
  if (!web_element.IsFormControlElement())
    return;
  const blink::WebFormControlElement control_element =
      web_element.ToConst<blink::WebFormControlElement>();
  UpdateFieldValueAndPropertiesMaskMap(control_element, nullptr,
                                       FieldPropertiesFlags::HAD_FOCUS,
                                       &field_value_and_properties_map_);
}

std::unique_ptr<PasswordForm> PasswordAutofillAgent::GetPasswordFormFromWebForm(
    const blink::WebFormElement& web_form) {
  return CreatePasswordFormFromWebForm(
      web_form, &field_value_and_properties_map_, &form_predictions_,
      &username_detector_cache_);
}

std::unique_ptr<PasswordForm>
PasswordAutofillAgent::GetPasswordFormFromUnownedInputElements() {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // The element's frame might have been detached in the meantime (see
  // http://crbug.com/585363, comments 5 and 6), in which case |frame| will
  // be null. This was hardly caused by form submission (unless the user is
  // supernaturally quick), so it is OK to drop the ball here.
  if (!frame)
    return nullptr;
  return CreatePasswordFormFromUnownedInputElements(
      *frame, &field_value_and_properties_map_, &form_predictions_,
      &username_detector_cache_);
}

// mojom::PasswordAutofillAgent:
void PasswordAutofillAgent::SetLoggingState(bool active) {
  logging_state_active_ = active;
}

void PasswordAutofillAgent::AutofillUsernameAndPasswordDataReceived(
    const FormsPredictionsMap& predictions) {
  form_predictions_.insert(predictions.begin(), predictions.end());
}

void PasswordAutofillAgent::FindFocusedPasswordForm(
    FindFocusedPasswordFormCallback callback) {
  std::unique_ptr<PasswordForm> password_form;

  blink::WebElement element =
      render_frame()->GetWebFrame()->GetDocument().FocusedElement();
  if (!element.IsNull() && element.HasHTMLTagName("input")) {
    blink::WebInputElement input = element.To<blink::WebInputElement>();
    if (input.IsPasswordField()) {
      if (!input.Form().IsNull()) {
        password_form = GetPasswordFormFromWebForm(input.Form());
      } else {
        password_form = GetPasswordFormFromUnownedInputElements();
        // Only try to use this form if |input| is one of the password elements
        // for |password_form|.
        if (password_form->password_element !=
                input.NameForAutofill().Utf16() &&
            password_form->new_password_element !=
                input.NameForAutofill().Utf16())
          password_form.reset();
      }
    }
  }

  if (!password_form)
    password_form.reset(new PasswordForm());

  password_form->submission_event =
      PasswordForm::SubmissionIndicatorEvent::MANUAL_SAVE;
  std::move(callback).Run(*password_form);
}

void PasswordAutofillAgent::BlacklistedFormFound() {
  blacklisted_form_found_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, private:

bool PasswordAutofillAgent::ShowSuggestionPopup(
    const PasswordInfo& password_info,
    const blink::WebInputElement& user_input,
    bool show_all,
    bool show_on_password_field) {
  DCHECK(!user_input.IsNull());
  blink::WebFrame* frame = user_input.GetDocument().GetFrame();
  if (!frame)
    return false;

  blink::WebView* webview = frame->View();
  if (!webview)
    return false;

  if (user_input.IsPasswordField() && !user_input.IsAutofilled() &&
      !user_input.Value().IsEmpty()) {
    GetAutofillDriver()->HidePopup();
    return false;
  }

  FormData form;
  FormFieldData field;
  form_util::FindFormAndFieldForFormControlElement(user_input, &form, &field);

  int options = 0;
  if (show_all)
    options |= SHOW_ALL;
  if (show_on_password_field)
    options |= IS_PASSWORD_FIELD;

  base::string16 username_string(user_input.IsPasswordField()
                                     ? base::string16()
                                     : user_input.Value().Utf16());

  GetPasswordManagerDriver()->ShowPasswordSuggestions(
      password_info.key, field.text_direction, username_string, options,
      render_frame()->GetRenderView()->ElementBoundsInWindow(user_input));
  username_query_prefix_ = username_string;
  return CanShowSuggestion(password_info.fill_data, username_string, show_all);
}

bool PasswordAutofillAgent::ShowManualFallbackSuggestion(
    const blink::WebInputElement& element) {
  if (!element.Value().IsEmpty()) {
    GetAutofillDriver()->HidePopup();
    return false;
  }

  FormData form;
  FormFieldData field;
  form_util::FindFormAndFieldForFormControlElement(element, &form, &field);
  GetPasswordManagerDriver()->ShowManualFallbackSuggestion(
      field.text_direction,
      render_frame()->GetRenderView()->ElementBoundsInWindow(element));
  return true;
}

void PasswordAutofillAgent::FrameClosing() {
  for (auto const& iter : web_input_to_password_info_) {
    password_to_username_.erase(iter.second.password_field);
  }
  web_input_to_password_info_.clear();
  if (form_element_observer_) {
    form_element_observer_->Disconnect();
    form_element_observer_ = nullptr;
  }
  provisionally_saved_form_.Reset();
  field_value_and_properties_map_.clear();
  username_detector_cache_.clear();
  sent_request_to_store_ = false;
  checked_safe_browsing_reputation_ = false;
  blacklisted_form_found_ = false;
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  page_passwords_analyser_.Reset();
#endif
}

void PasswordAutofillAgent::ClearPreview(
    blink::WebInputElement* username,
    blink::WebInputElement* password) {
  if (!username->IsNull() && !username->SuggestedValue().IsEmpty()) {
    username->SetSuggestedValue(blink::WebString());
    username->SetAutofilled(was_username_autofilled_);
    username->SetSelectionRange(username_query_prefix_.length(),
                                username->Value().length());
  }
  if (!password->SuggestedValue().IsEmpty()) {
    password->SetSuggestedValue(blink::WebString());
    password->SetAutofilled(was_password_autofilled_);
  }
}
void PasswordAutofillAgent::ProvisionallySavePassword(
    const blink::WebFormElement& form,
    const blink::WebInputElement& element,
    ProvisionallySaveRestriction restriction) {
  DCHECK(!form.IsNull() || !element.IsNull());

  std::unique_ptr<PasswordForm> password_form;
  if (form.IsNull()) {
    password_form = GetPasswordFormFromUnownedInputElements();
  } else {
    password_form = GetPasswordFormFromWebForm(form);
  }
  if (!password_form)
    return;

  bool has_password = !password_form->password_value.empty() ||
                      !password_form->new_password_value.empty();
  if (restriction == RESTRICTION_NON_EMPTY_PASSWORD && !has_password)
    return;

  if (!FrameCanAccessPasswordManager())
    return;

  provisionally_saved_form_.Set(std::move(password_form), form, element);

  if (base::FeatureList::IsEnabled(
          password_manager::features::kEnableManualSaving)) {
    if (has_password) {
      GetPasswordManagerDriver()->ShowManualFallbackForSaving(
          provisionally_saved_form_.password_form());
    } else {
      GetPasswordManagerDriver()->HideManualFallbackForSaving();
    }
  }
}

bool PasswordAutofillAgent::FillUserNameAndPassword(
    blink::WebInputElement* username_element,
    blink::WebInputElement* password_element,
    const PasswordFormFillData& fill_data,
    bool exact_username_match,
    bool set_selection,
    FieldValueAndPropertiesMaskMap* field_value_and_properties_map,
    base::Callback<void(blink::WebInputElement*)> registration_callback,
    RendererSavePasswordProgressLogger* logger) {
  if (logger)
    logger->LogMessage(Logger::STRING_FILL_USERNAME_AND_PASSWORD_METHOD);

  // Don't fill username if password can't be set.
  if (!IsElementAutocompletable(*password_element))
    return false;

  base::string16 current_username;
  if (!username_element->IsNull()) {
    current_username = username_element->Value().Utf16();
  }

  // username and password will contain the match found if any.
  base::string16 username;
  base::string16 password;

  FindMatchesByUsername(fill_data, current_username, exact_username_match,
                        logger, &username, &password);

  if (password.empty())
    return false;

  // TODO(tkent): Check maxlength and pattern for both username and password
  // fields.

  // Call OnFieldAutofilled before WebInputElement::SetAutofilled which may
  // cause frame closing.
  if (password_generation_agent_)
    password_generation_agent_->OnFieldAutofilled(*password_element);

  // Input matches the username, fill in required values.
  if (!username_element->IsNull() &&
      IsElementAutocompletable(*username_element)) {
    // TODO(crbug.com/507714): Why not setSuggestedValue?
    if (username_element->Value().Utf16() != username)
      username_element->SetAutofillValue(blink::WebString::FromUTF16(username));
    UpdateFieldValueAndPropertiesMaskMap(*username_element, &username,
                                         FieldPropertiesFlags::AUTOFILLED,
                                         field_value_and_properties_map);
    username_element->SetAutofilled(true);
    if (logger)
      logger->LogElementName(Logger::STRING_USERNAME_FILLED, *username_element);
    if (set_selection) {
      form_util::PreviewSuggestion(username, current_username,
                                   username_element);
    }
  }

  // Wait to fill in the password until a user gesture occurs. This is to make
  // sure that we do not fill in the DOM with a password until we believe the
  // user is intentionally interacting with the page.
  if (password_element->Value().Utf16() != password)
    password_element->SetSuggestedValue(blink::WebString::FromUTF16(password));
  UpdateFieldValueAndPropertiesMaskMap(*password_element, &password,
                                       FieldPropertiesFlags::AUTOFILLED,
                                       field_value_and_properties_map);
  ProvisionallySavePassword(password_element->Form(), *password_element,
                            RESTRICTION_NONE);
  registration_callback.Run(password_element);
  password_element->SetAutofilled(true);

  if (logger)
    logger->LogElementName(Logger::STRING_PASSWORD_FILLED, *password_element);
  return true;
}

bool PasswordAutofillAgent::FillFormOnPasswordReceived(
    const PasswordFormFillData& fill_data,
    blink::WebInputElement username_element,
    blink::WebInputElement password_element,
    FieldValueAndPropertiesMaskMap* field_value_and_properties_map,
    base::Callback<void(blink::WebInputElement*)> registration_callback,
    RendererSavePasswordProgressLogger* logger) {
  // Do not fill if the password field is in a chain of iframes not having
  // identical origin.
  blink::WebFrame* cur_frame = password_element.GetDocument().GetFrame();
  blink::WebString bottom_frame_origin =
      cur_frame->GetSecurityOrigin().ToString();

  DCHECK(cur_frame);

  while (cur_frame->Parent()) {
    cur_frame = cur_frame->Parent();
    if (!IsPublicSuffixDomainMatch(
            bottom_frame_origin.Utf8(),
            cur_frame->GetSecurityOrigin().ToString().Utf8()))
      return false;
  }

  // If we can't modify the password, don't try to set the username
  if (!IsElementAutocompletable(password_element))
    return false;

  bool form_contains_fillable_username_field =
      FillDataContainsFillableUsername(fill_data);
  bool ambiguous_or_empty_names =
      DoesFormContainAmbiguousOrEmptyNames(fill_data);
  base::string16 username_field_name;
  if (form_contains_fillable_username_field)
    username_field_name =
        FieldName(fill_data.username_field, ambiguous_or_empty_names);

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
  bool form_has_fillable_username = !username_field_name.empty() &&
                                    IsElementAutocompletable(username_element);

  if (form_has_fillable_username && username_element.Value().IsEmpty()) {
    // TODO(tkent): Check maxlength and pattern.
    username_element.SetAutofillValue(
        blink::WebString::FromUTF16(fill_data.username_field.value));
  }

  bool exact_username_match =
      username_element.IsNull() || IsElementEditable(username_element);
  // Use the exact match for the editable username fields and allow prefix
  // match for read-only username fields.
  return FillUserNameAndPassword(
      &username_element, &password_element, fill_data, exact_username_match,
      false /* set_selection */, field_value_and_properties_map,
      registration_callback, logger);
}

const mojom::AutofillDriverPtr& PasswordAutofillAgent::GetAutofillDriver() {
  DCHECK(autofill_agent_);
  return autofill_agent_->GetAutofillDriver();
}

const mojom::PasswordManagerDriverPtr&
PasswordAutofillAgent::GetPasswordManagerDriver() {
  if (!password_manager_driver_) {
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&password_manager_driver_));
  }

  return password_manager_driver_;
}

}  // namespace autofill
