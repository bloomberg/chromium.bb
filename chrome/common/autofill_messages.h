// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <string>

#include "base/time.h"
#include "chrome/common/common_param_traits_macros.h"
#include "chrome/common/form_data.h"
#include "chrome/common/form_data_predictions.h"
#include "chrome/common/form_field_data.h"
#include "chrome/common/form_field_data_predictions.h"
#include "chrome/common/password_form_fill_data.h"
#include "content/public/common/common_param_traits_macros.h"
#include "content/public/common/password_form.h"
#include "content/public/common/ssl_status.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "ui/gfx/rect.h"

#define IPC_MESSAGE_START AutofillMsgStart

IPC_STRUCT_TRAITS_BEGIN(FormFieldData)
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
  IPC_STRUCT_TRAITS_MEMBER(option_values)
  IPC_STRUCT_TRAITS_MEMBER(option_contents)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(FormFieldDataPredictions)
  IPC_STRUCT_TRAITS_MEMBER(field)
  IPC_STRUCT_TRAITS_MEMBER(signature)
  IPC_STRUCT_TRAITS_MEMBER(heuristic_type)
  IPC_STRUCT_TRAITS_MEMBER(server_type)
  IPC_STRUCT_TRAITS_MEMBER(overall_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(FormData)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(user_submitted)
  IPC_STRUCT_TRAITS_MEMBER(fields)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(FormDataPredictions)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(signature)
  IPC_STRUCT_TRAITS_MEMBER(experiment_id)
  IPC_STRUCT_TRAITS_MEMBER(fields)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PasswordFormFillData)
  IPC_STRUCT_TRAITS_MEMBER(basic_data)
  IPC_STRUCT_TRAITS_MEMBER(additional_logins)
  IPC_STRUCT_TRAITS_MEMBER(wait_for_username)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS(WebKit::WebFormElement::AutocompleteResult)

// Autofill messages sent from the browser to the renderer.

// Reply to the AutofillHostMsg_QueryFormFieldAutofill message with the
// Autofill suggestions.
IPC_MESSAGE_ROUTED5(AutofillMsg_SuggestionsReturned,
                    int /* id of the request message */,
                    std::vector<string16> /* names */,
                    std::vector<string16> /* labels */,
                    std::vector<string16> /* icons */,
                    std::vector<int> /* unique_ids */)

// Reply to the AutofillHostMsg_FillAutofillFormData message with the
// Autofill form data.
IPC_MESSAGE_ROUTED2(AutofillMsg_FormDataFilled,
                    int /* id of the request message */,
                    FormData /* form data */)

// Fill a password form and prepare field autocomplete for multiple
// matching logins. Lets the renderer know if it should disable the popup
// because the browser process will own the popup UI.
IPC_MESSAGE_ROUTED2(AutofillMsg_FillPasswordForm,
                    PasswordFormFillData, /* the fill form data*/
                    bool /* disable popup */ )

// Send the heuristic and server field type predictions to the renderer.
IPC_MESSAGE_ROUTED1(
    AutofillMsg_FieldTypePredictionsAvailable,
    std::vector<FormDataPredictions> /* forms */)

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
                    string16 /* new node text */)

// Sets the currently selected node's value to be the given data list value.
IPC_MESSAGE_ROUTED1(AutofillMsg_AcceptDataListSuggestion,
                    string16 /* accepted data list value */)

// Tells the renderer to populate the correct password fields with this
// generated password.
IPC_MESSAGE_ROUTED1(AutofillMsg_GeneratedPasswordAccepted,
                    string16 /* generated_password */)

// Tells the renderer whether password generation is enabled.
IPC_MESSAGE_ROUTED1(AutofillMsg_PasswordGenerationEnabled,
                    bool /* is_enabled */)

// Tells the renderer that the password field has accept the suggestion.
IPC_MESSAGE_ROUTED1(AutofillMsg_AcceptPasswordAutofillSuggestion,
                    string16 /* username value*/)

// Tells the renderer that this password form is not blacklisted.  A form can
// be blacklisted if a user chooses "never save passwords for this site".
IPC_MESSAGE_ROUTED1(AutofillMsg_FormNotBlacklisted,
                    content::PasswordForm /* form checked */)

// Sent when requestAutocomplete() succeeds. Tells the renderer to Autofill the
// form that requested autocomplete with the |form_data| values input by the
// user.
IPC_MESSAGE_ROUTED1(AutofillMsg_RequestAutocompleteSuccess,
                    FormData /* form_data */)

// Sent when requestAutocomplete() fails. Currently, this happens when a form is
// requested to be autocompleted with no input or select tags with autocomplete
// attributes.
IPC_MESSAGE_ROUTED0(AutofillMsg_RequestAutocompleteError)

// Autofill messages sent from the renderer to the browser.

// TODO(creis): check in the browser that the renderer actually has permission
// for the URL to avoid compromised renderers talking to the browser.

// Notification that forms have been seen that are candidates for
// filling/submitting by the AutofillManager.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_FormsSeen,
                    std::vector<FormData> /* forms */,
                    base::TimeTicks /* timestamp */)

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
                    FormData /* form */,
                    base::TimeTicks /* timestamp */)

// Notification that a form field's value has changed.
IPC_MESSAGE_ROUTED3(AutofillHostMsg_TextFieldDidChange,
                    FormData /* the form */,
                    FormFieldData /* the form field */,
                    base::TimeTicks /* timestamp */)

// Queries the browser for Autofill suggestions for a form input field.
IPC_MESSAGE_ROUTED5(AutofillHostMsg_QueryFormFieldAutofill,
                    int /* id of this message */,
                    FormData /* the form */,
                    FormFieldData /* the form field */,
                    gfx::Rect /* input field bounds, window-relative */,
                    bool /* display warning if autofill disabled */)

// Sent when the popup with Autofill suggestions for a form is shown.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_DidShowAutofillSuggestions,
                    bool /* is this a new popup? */)

// Instructs the browser to fill in the values for a form using Autofill
// profile data.
IPC_MESSAGE_ROUTED4(AutofillHostMsg_FillAutofillFormData,
                    int /* id of this message */,
                    FormData /* the form  */,
                    FormFieldData /* the form field  */,
                    int /* profile unique ID */)

// Sent when a form is previewed with Autofill suggestions.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_DidPreviewAutofillFormData)

// Sent when a form is filled with Autofill suggestions.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_DidFillAutofillFormData,
                    base::TimeTicks /* timestamp */)

// Sent when a form receives a request to do interactive autocomplete.
IPC_MESSAGE_ROUTED3(AutofillHostMsg_RequestAutocomplete,
                    FormData /* form_data */,
                    GURL /* frame_url */,
                    content::SSLStatus /* ssl_status */)

// Instructs the browser to remove the specified Autocomplete entry from the
// database.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_RemoveAutocompleteEntry,
                    string16 /* field name */,
                    string16 /* value */)

// Instructs the browser to show the Autofill dialog.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_ShowAutofillDialog)

// Send when a text field is done editing.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_DidEndTextFieldEditing)

// Instructs the browser to hide the Autofill popup.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_HideAutofillPopup)

// Instructs the browser to show the password generation bubble at the
// specified location. This location should be specified in the renderers
// coordinate system. Form is the form associated with the password field.
IPC_MESSAGE_ROUTED3(AutofillHostMsg_ShowPasswordGenerationPopup,
                    gfx::Rect /* source location */,
                    int /* max length of the password */,
                    content::PasswordForm)

// Instruct the browser that a password mapping has been found for a field.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_AddPasswordFormMapping,
                    FormFieldData, /* the user name field */
                    PasswordFormFillData /* password pairings */)

// Instruct the browser to show a popup with the following suggestions from the
// password manager.
IPC_MESSAGE_ROUTED3(AutofillHostMsg_ShowPasswordSuggestions,
                    FormFieldData /* the form field */,
                    gfx::Rect /* input field bounds, window-relative */,
                    std::vector<string16> /* suggestions */)

// Inform browser of data list values for the curent field.
IPC_MESSAGE_ROUTED4(AutofillHostMsg_SetDataList,
                    std::vector<string16> /* values */,
                    std::vector<string16> /* labels */,
                    std::vector<string16> /* icons */,
                    std::vector<int> /* unique ids */)
