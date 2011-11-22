// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <string>

#include "base/time.h"
#include "content/public/common/webkit_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_data_predictions.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/form_field_predictions.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/password_form_dom_manager.h"

#define IPC_MESSAGE_START AutofillMsgStart

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::FormField)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(form_control_type)
  IPC_STRUCT_TRAITS_MEMBER(autocomplete_type)
  IPC_STRUCT_TRAITS_MEMBER(max_length)
  IPC_STRUCT_TRAITS_MEMBER(is_autofilled)
  IPC_STRUCT_TRAITS_MEMBER(is_focusable)
  IPC_STRUCT_TRAITS_MEMBER(should_autocomplete)
  IPC_STRUCT_TRAITS_MEMBER(option_values)
  IPC_STRUCT_TRAITS_MEMBER(option_contents)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::FormFieldPredictions)
  IPC_STRUCT_TRAITS_MEMBER(field)
  IPC_STRUCT_TRAITS_MEMBER(signature)
  IPC_STRUCT_TRAITS_MEMBER(heuristic_type)
  IPC_STRUCT_TRAITS_MEMBER(server_type)
  IPC_STRUCT_TRAITS_MEMBER(overall_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::FormData)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(user_submitted)
  IPC_STRUCT_TRAITS_MEMBER(fields)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::FormDataPredictions)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(signature)
  IPC_STRUCT_TRAITS_MEMBER(experiment_id)
  IPC_STRUCT_TRAITS_MEMBER(fields)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::PasswordFormFillData)
  IPC_STRUCT_TRAITS_MEMBER(basic_data)
  IPC_STRUCT_TRAITS_MEMBER(additional_logins)
  IPC_STRUCT_TRAITS_MEMBER(wait_for_username)
IPC_STRUCT_TRAITS_END()

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
                    webkit_glue::FormData /* form data */)

// Fill a password form and prepare field autocomplete for multiple
// matching logins.
IPC_MESSAGE_ROUTED1(AutofillMsg_FillPasswordForm,
                    webkit_glue::PasswordFormFillData)

// Send the heuristic and server field type predictions to the renderer.
IPC_MESSAGE_ROUTED1(
    AutofillMsg_FieldTypePredictionsAvailable,
    std::vector<webkit_glue::FormDataPredictions> /* forms */)

// Select an Autofill item when using an external delegate.
IPC_MESSAGE_ROUTED1(AutofillMsg_SelectAutofillSuggestionAtIndex,
                    int /* listIndex */)

// Autofill messages sent from the renderer to the browser.

// Notification that forms have been seen that are candidates for
// filling/submitting by the AutofillManager.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_FormsSeen,
                    std::vector<webkit_glue::FormData> /* forms */,
                    base::TimeTicks /* timestamp */)

// Notification that password forms have been seen that are candidates for
// filling/submitting by the password manager.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_PasswordFormsFound,
                    std::vector<webkit_glue::PasswordForm> /* forms */)

// Notification that initial layout has occurred and the following password
// forms are visible on the page (e.g. not set to display:none.)
IPC_MESSAGE_ROUTED1(AutofillHostMsg_PasswordFormsVisible,
                    std::vector<webkit_glue::PasswordForm> /* forms */)

// Notification that a form has been submitted.  The user hit the button.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_FormSubmitted,
                    webkit_glue::FormData /* form */,
                    base::TimeTicks /* timestamp */)

// Notification that a form field's value has changed.
IPC_MESSAGE_ROUTED3(AutofillHostMsg_TextFieldDidChange,
                    webkit_glue::FormData /* the form */,
                    webkit_glue::FormField /* the form field */,
                    base::TimeTicks /* timestamp */)

// Queries the browser for Autofill suggestions for a form input field.
IPC_MESSAGE_ROUTED5(AutofillHostMsg_QueryFormFieldAutofill,
                    int /* id of this message */,
                    webkit_glue::FormData /* the form */,
                    webkit_glue::FormField /* the form field */,
                    gfx::Rect /* input field bounds, window-relative */,
                    bool /* display warning if autofill disabled */)

// Sent when the popup with Autofill suggestions for a form is shown.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_DidShowAutofillSuggestions,
                    bool /* is this a new popup? */)

// Instructs the browser to fill in the values for a form using Autofill
// profile data.
IPC_MESSAGE_ROUTED4(AutofillHostMsg_FillAutofillFormData,
                    int /* id of this message */,
                    webkit_glue::FormData /* the form  */,
                    webkit_glue::FormField /* the form field  */,
                    int /* profile unique ID */)

// Sent when a form is previewed with Autofill suggestions.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_DidPreviewAutofillFormData)

// Sent when a form is filled with Autofill suggestions.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_DidFillAutofillFormData,
                    base::TimeTicks /* timestamp */)

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
