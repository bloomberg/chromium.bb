// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <string>

#include "base/time/time.h"
#include "components/autofill/core/common/autocheckout_status.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/forms_seen_state.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/web_element_descriptor.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/common_param_traits_macros.h"
#include "content/public/common/password_form.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START AutofillMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(autofill::AutocheckoutStatus,
                          autofill::AUTOCHECKOUT_STATUS_NUM_STATUS - 1)
IPC_ENUM_TRAITS_MAX_VALUE(autofill::FormsSeenState,
                          autofill::FORMS_SEEN_STATE_NUM_STATES - 1)
IPC_ENUM_TRAITS_MAX_VALUE(base::i18n::TextDirection,
                          base::i18n::TEXT_DIRECTION_NUM_DIRECTIONS - 1)

IPC_STRUCT_TRAITS_BEGIN(autofill::WebElementDescriptor)
  IPC_STRUCT_TRAITS_MEMBER(descriptor)
  IPC_STRUCT_TRAITS_MEMBER(retrieval_method)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS(autofill::WebElementDescriptor::RetrievalMethod)

IPC_STRUCT_TRAITS_BEGIN(autofill::FormFieldData)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(form_control_type)
  IPC_STRUCT_TRAITS_MEMBER(autocomplete_attribute)
  IPC_STRUCT_TRAITS_MEMBER(max_length)
  IPC_STRUCT_TRAITS_MEMBER(is_autofilled)
  IPC_STRUCT_TRAITS_MEMBER(is_checked)
  IPC_STRUCT_TRAITS_MEMBER(is_checkable)
  IPC_STRUCT_TRAITS_MEMBER(is_focusable)
  IPC_STRUCT_TRAITS_MEMBER(should_autocomplete)
  IPC_STRUCT_TRAITS_MEMBER(text_direction)
  IPC_STRUCT_TRAITS_MEMBER(option_values)
  IPC_STRUCT_TRAITS_MEMBER(option_contents)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(autofill::FormFieldDataPredictions)
  IPC_STRUCT_TRAITS_MEMBER(field)
  IPC_STRUCT_TRAITS_MEMBER(signature)
  IPC_STRUCT_TRAITS_MEMBER(heuristic_type)
  IPC_STRUCT_TRAITS_MEMBER(server_type)
  IPC_STRUCT_TRAITS_MEMBER(overall_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(autofill::FormData)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(user_submitted)
  IPC_STRUCT_TRAITS_MEMBER(fields)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(autofill::FormDataPredictions)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(signature)
  IPC_STRUCT_TRAITS_MEMBER(experiment_id)
  IPC_STRUCT_TRAITS_MEMBER(fields)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(autofill::UsernamesCollectionKey)
  IPC_STRUCT_TRAITS_MEMBER(username)
  IPC_STRUCT_TRAITS_MEMBER(password)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(autofill::PasswordFormFillData)
  IPC_STRUCT_TRAITS_MEMBER(basic_data)
  IPC_STRUCT_TRAITS_MEMBER(preferred_realm)
  IPC_STRUCT_TRAITS_MEMBER(additional_logins)
  IPC_STRUCT_TRAITS_MEMBER(other_possible_usernames)
  IPC_STRUCT_TRAITS_MEMBER(wait_for_username)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(autofill::PasswordAndRealm)
  IPC_STRUCT_TRAITS_MEMBER(password)
  IPC_STRUCT_TRAITS_MEMBER(realm)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS(WebKit::WebFormElement::AutocompleteResult)

// Autofill messages sent from the browser to the renderer.

// Request to parse all the forms without field count limit.
IPC_MESSAGE_ROUTED0(AutofillMsg_GetAllForms)

// Reply to the AutofillHostMsg_FillAutofillFormData message with the
// Autofill form data.
IPC_MESSAGE_ROUTED2(AutofillMsg_FormDataFilled,
                    int /* id of the request message */,
                    autofill::FormData /* form data */)

// Fill a password form and prepare field autocomplete for multiple
// matching logins. Lets the renderer know if it should disable the popup
// because the browser process will own the popup UI.
IPC_MESSAGE_ROUTED1(AutofillMsg_FillPasswordForm,
                    autofill::PasswordFormFillData /* the fill form data*/)

// Send the heuristic and server field type predictions to the renderer.
IPC_MESSAGE_ROUTED1(
    AutofillMsg_FieldTypePredictionsAvailable,
    std::vector<autofill::FormDataPredictions> /* forms */)

// Tells the renderer that the next form will be filled for real.
IPC_MESSAGE_ROUTED0(AutofillMsg_SetAutofillActionFill)

// Clears the currently displayed Autofill results.
IPC_MESSAGE_ROUTED0(AutofillMsg_ClearForm)

// Tells the renderer that the next form will be filled as a preview.
IPC_MESSAGE_ROUTED0(AutofillMsg_SetAutofillActionPreview)

// Tells the renderer that the Autofill previewed form should be cleared.
IPC_MESSAGE_ROUTED0(AutofillMsg_ClearPreviewedForm)

// Sets the currently selected node's value.
IPC_MESSAGE_ROUTED1(AutofillMsg_SetNodeText,
                    base::string16 /* new node text */)

// Sets the currently selected node's value to be the given data list value.
IPC_MESSAGE_ROUTED1(AutofillMsg_AcceptDataListSuggestion,
                    base::string16 /* accepted data list value */)

// Tells the renderer to populate the correct password fields with this
// generated password.
IPC_MESSAGE_ROUTED1(AutofillMsg_GeneratedPasswordAccepted,
                    base::string16 /* generated_password */)

// Tells the renderer whether password generation is enabled.
IPC_MESSAGE_ROUTED1(AutofillMsg_PasswordGenerationEnabled,
                    bool /* is_enabled */)

// Tells the renderer that the password field has accept the suggestion.
IPC_MESSAGE_ROUTED1(AutofillMsg_AcceptPasswordAutofillSuggestion,
                    base::string16 /* username value*/)

// Tells the renderer that this password form is not blacklisted.  A form can
// be blacklisted if a user chooses "never save passwords for this site".
IPC_MESSAGE_ROUTED1(AutofillMsg_FormNotBlacklisted,
                    content::PasswordForm /* form checked */)

// Sent when requestAutocomplete() finishes (either succesfully or with an
// error). If it was a success, the renderer fills the form that requested
// autocomplete with the |form_data| values input by the user.
IPC_MESSAGE_ROUTED2(AutofillMsg_RequestAutocompleteResult,
                    WebKit::WebFormElement::AutocompleteResult /* result */,
                    autofill::FormData /* form_data */)

// Sent when a page should be filled using Autocheckout. This happens when the
// Autofill server hints that a page is Autocheckout enabled.
IPC_MESSAGE_ROUTED4(AutofillMsg_FillFormsAndClick,
                    std::vector<autofill::FormData> /* form_data */,
                    std::vector<autofill::WebElementDescriptor> /*
                        click_elements_before_form_fill */,
                    std::vector<autofill::WebElementDescriptor> /*
                        click_elements_after_form_fill */,
                    autofill::WebElementDescriptor /* element_descriptor */)

// Sent when Autocheckout is supported for the current page. The page has to
// be whitelisted and the Autofill server must have returned Autocheckout page
// metadata.
IPC_MESSAGE_ROUTED0(AutofillMsg_AutocheckoutSupported)

// Sent when the current page is actually displayed in the browser, possibly
// after being preloaded.
IPC_MESSAGE_ROUTED0(AutofillMsg_PageShown)

// Autofill messages sent from the renderer to the browser.

// TODO(creis): check in the browser that the renderer actually has permission
// for the URL to avoid compromised renderers talking to the browser.

// Notification that forms have been seen that are candidates for
// filling/submitting by the AutofillManager.
IPC_MESSAGE_ROUTED3(AutofillHostMsg_FormsSeen,
                    std::vector<autofill::FormData> /* forms */,
                    base::TimeTicks /* timestamp */,
                    autofill::FormsSeenState /* state */)

// Notification that password forms have been seen that are candidates for
// filling/submitting by the password manager.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_PasswordFormsParsed,
                    std::vector<content::PasswordForm> /* forms */)

// Notification that initial layout has occurred and the following password
// forms are visible on the page (e.g. not set to display:none.)
IPC_MESSAGE_ROUTED1(AutofillHostMsg_PasswordFormsRendered,
                    std::vector<content::PasswordForm> /* forms */)

// Notification that a form has been submitted.  The user hit the button.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_FormSubmitted,
                    autofill::FormData /* form */,
                    base::TimeTicks /* timestamp */)

// Notification that a form field's value has changed.
IPC_MESSAGE_ROUTED3(AutofillHostMsg_TextFieldDidChange,
                    autofill::FormData /* the form */,
                    autofill::FormFieldData /* the form field */,
                    base::TimeTicks /* timestamp */)

// Shows the Autocheckout bubble if the conditions are right.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_MaybeShowAutocheckoutBubble,
                    autofill::FormData /* form */,
                    gfx::RectF /* bounding_box */)

// Queries the browser for Autofill suggestions for a form input field.
IPC_MESSAGE_ROUTED5(AutofillHostMsg_QueryFormFieldAutofill,
                    int /* id of this message */,
                    autofill::FormData /* the form */,
                    autofill::FormFieldData /* the form field */,
                    gfx::RectF /* input field bounds, window-relative */,
                    bool /* display warning if autofill disabled */)

// Instructs the browser to fill in the values for a form using Autofill
// profile data.
IPC_MESSAGE_ROUTED4(AutofillHostMsg_FillAutofillFormData,
                    int /* id of this message */,
                    autofill::FormData /* the form  */,
                    autofill::FormFieldData /* the form field  */,
                    int /* profile unique ID */)

// Sent when a form is previewed with Autofill suggestions.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_DidPreviewAutofillFormData)

// Sent when a form is filled with Autofill suggestions.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_DidFillAutofillFormData,
                    base::TimeTicks /* timestamp */)

// Sent when a form receives a request to do interactive autocomplete.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_RequestAutocomplete,
                    autofill::FormData /* form_data */,
                    GURL /* frame_url */)

// Instructs the browser to show the Autofill dialog.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_ShowAutofillDialog)

// Send when a text field is done editing.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_DidEndTextFieldEditing)

// Instructs the browser to hide the Autofill UI.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_HideAutofillUI)

// Sent when the renderer filled an Autocheckout page and clicked the proceed
// button or if there was an error.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_AutocheckoutPageCompleted,
                    autofill::AutocheckoutStatus /* status */)

// Instructs the browser to show the password generation bubble at the
// specified location. This location should be specified in the renderers
// coordinate system. Form is the form associated with the password field.
IPC_MESSAGE_ROUTED3(AutofillHostMsg_ShowPasswordGenerationPopup,
                    gfx::Rect /* source location */,
                    int /* max length of the password */,
                    content::PasswordForm)

// Instruct the browser that a password mapping has been found for a field.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_AddPasswordFormMapping,
                    autofill::FormFieldData, /* the user name field */
                    autofill::PasswordFormFillData /* password pairings */)

// Instruct the browser to show a popup with the following suggestions from the
// password manager.
IPC_MESSAGE_ROUTED4(AutofillHostMsg_ShowPasswordSuggestions,
                    autofill::FormFieldData /* the form field */,
                    gfx::RectF /* input field bounds, window-relative */,
                    std::vector<base::string16> /* suggestions */,
                    std::vector<base::string16> /* realms */)

// Inform browser of data list values for the curent field.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_SetDataList,
                    std::vector<base::string16> /* values */,
                    std::vector<base::string16> /* labels */)
