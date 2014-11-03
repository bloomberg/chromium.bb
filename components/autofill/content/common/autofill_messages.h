// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/autofill/content/common/autofill_param_traits_macros.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/web_element_descriptor.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START AutofillMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(base::i18n::TextDirection,
                          base::i18n::TEXT_DIRECTION_NUM_DIRECTIONS - 1)

IPC_STRUCT_TRAITS_BEGIN(autofill::WebElementDescriptor)
  IPC_STRUCT_TRAITS_MEMBER(descriptor)
  IPC_STRUCT_TRAITS_MEMBER(retrieval_method)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(autofill::WebElementDescriptor::RetrievalMethod,
                          autofill::WebElementDescriptor::NONE)

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

IPC_ENUM_TRAITS_MAX_VALUE(
    blink::WebFormElement::AutocompleteResult,
    blink::WebFormElement::AutocompleteResultErrorInvalid)

// Autofill messages sent from the browser to the renderer.

// Instructs the renderer to immediately return an IPC acknowledging the ping.
// This is used to correctly sequence events, since IPCs are guaranteed to be
// processed in order.
IPC_MESSAGE_ROUTED0(AutofillMsg_Ping)

// Instructs the renderer to fill the active form with the given form data.
IPC_MESSAGE_ROUTED2(AutofillMsg_FillForm,
                    int /* query_id */,
                    autofill::FormData /* form */)

// Instructs the renderer to preview the active form with the given form data.
IPC_MESSAGE_ROUTED2(AutofillMsg_PreviewForm,
                    int /* query_id */,
                    autofill::FormData /* form */)

// Fill a password form and prepare field autocomplete for multiple
// matching logins. Lets the renderer know if it should disable the popup
// because the browser process will own the popup UI.
IPC_MESSAGE_ROUTED1(AutofillMsg_FillPasswordForm,
                    autofill::PasswordFormFillData /* the fill form data*/)

// Notification to start (|active| == true) or stop (|active| == false) logging
// the decisions made about saving the password.
IPC_MESSAGE_ROUTED1(AutofillMsg_SetLoggingState, bool /* active */)

// Send the heuristic and server field type predictions to the renderer.
IPC_MESSAGE_ROUTED1(
    AutofillMsg_FieldTypePredictionsAvailable,
    std::vector<autofill::FormDataPredictions> /* forms */)

// Clears the currently displayed Autofill results.
IPC_MESSAGE_ROUTED0(AutofillMsg_ClearForm)

// Tells the renderer that the Autofill previewed form should be cleared.
IPC_MESSAGE_ROUTED0(AutofillMsg_ClearPreviewedForm)

// Sets the currently selected node's value.
IPC_MESSAGE_ROUTED1(AutofillMsg_FillFieldWithValue,
                    base::string16 /* value */)

// Sets the suggested value for the currently previewed node.
IPC_MESSAGE_ROUTED1(AutofillMsg_PreviewFieldWithValue,
                    base::string16 /* value */)

// Sets the currently selected node's value to be the given data list value.
IPC_MESSAGE_ROUTED1(AutofillMsg_AcceptDataListSuggestion,
                    base::string16 /* accepted data list value */)

// Tells the renderer to populate the correct password fields with this
// generated password.
IPC_MESSAGE_ROUTED1(AutofillMsg_GeneratedPasswordAccepted,
                    base::string16 /* generated_password */)

// Tells the renderer to fill the username and password with with given
// values.
IPC_MESSAGE_ROUTED2(AutofillMsg_FillPasswordSuggestion,
                    base::string16 /* username */,
                    base::string16 /* password */)

// Tells the renderer to preview the username and password with the given
// values.
IPC_MESSAGE_ROUTED2(AutofillMsg_PreviewPasswordSuggestion,
                    base::string16 /* username */,
                    base::string16 /* password */)

// Tells the renderer that this password form is not blacklisted.  A form can
// be blacklisted if a user chooses "never save passwords for this site".
IPC_MESSAGE_ROUTED1(AutofillMsg_FormNotBlacklisted,
                    autofill::PasswordForm /* form checked */)

// Sent when requestAutocomplete() finishes (either succesfully or with an
// error). If it was a success, the renderer fills the form that requested
// autocomplete with the |form_data| values input by the user. |message|
// is printed to the console if non-empty.
IPC_MESSAGE_ROUTED3(AutofillMsg_RequestAutocompleteResult,
                    blink::WebFormElement::AutocompleteResult /* result */,
                    base::string16 /* message */,
                    autofill::FormData /* form_data */)

// Sent when Autofill manager gets the query response from the Autofill server
// and there are fields classified as ACCOUNT_CREATION_PASSWORD in the response.
IPC_MESSAGE_ROUTED1(AutofillMsg_AccountCreationFormsDetected,
                    std::vector<autofill::FormData> /* forms */)

// Autofill messages sent from the renderer to the browser.

// TODO(creis): check in the browser that the renderer actually has permission
// for the URL to avoid compromised renderers talking to the browser.

// Notification that forms have been seen that are candidates for
// filling/submitting by the AutofillManager.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_FormsSeen,
                    std::vector<autofill::FormData> /* forms */,
                    base::TimeTicks /* timestamp */)

// Notification that password forms have been seen that are candidates for
// filling/submitting by the password manager.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_PasswordFormsParsed,
                    std::vector<autofill::PasswordForm> /* forms */)

// Notification that initial layout has occurred and the following password
// forms are visible on the page (e.g. not set to display:none.), and whether
// all frames in the page have been rendered.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_PasswordFormsRendered,
                    std::vector<autofill::PasswordForm> /* forms */,
                    bool /* did_stop_loading */)

// A ping to the browser that PasswordAutofillAgent was constructed. As a
// consequence, the browser sends AutofillMsg_SetLoggingState with the current
// state of the logging activity.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_PasswordAutofillAgentConstructed)

// Notification that this password form was submitted by the user.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_PasswordFormSubmitted,
                    autofill::PasswordForm /* form */)

// Sends |log| to browser for displaying to the user. Only strings passed as an
// argument to methods overriding SavePasswordProgressLogger::SendLog may become
// |log|, because those are guaranteed to be sanitized. Never pass a free-form
// string as |log|.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_RecordSavePasswordProgress,
                    std::string /* log */)

// Notification that a form has been submitted.  The user hit the button.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_FormSubmitted,
                    autofill::FormData /* form */,
                    base::TimeTicks /* timestamp */)

// Notification that a form field's value has changed.
IPC_MESSAGE_ROUTED3(AutofillHostMsg_TextFieldDidChange,
                    autofill::FormData /* the form */,
                    autofill::FormFieldData /* the form field */,
                    base::TimeTicks /* timestamp */)

// Queries the browser for Autofill suggestions for a form input field.
IPC_MESSAGE_ROUTED5(AutofillHostMsg_QueryFormFieldAutofill,
                    int /* id of this message */,
                    autofill::FormData /* the form */,
                    autofill::FormFieldData /* the form field */,
                    gfx::RectF /* input field bounds, window-relative */,
                    bool /* display warning if autofill disabled */)

// Sent when a form is previewed with Autofill suggestions.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_DidPreviewAutofillFormData)

// Sent immediately after the renderer receives a ping IPC.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_PingAck)

// Sent when a form is filled with Autofill suggestions.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_DidFillAutofillFormData,
                    base::TimeTicks /* timestamp */)

// Sent when a form receives a request to do interactive autocomplete.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_RequestAutocomplete,
                    autofill::FormData /* form_data */,
                    GURL /* frame_url */)

// Sent when interactive autocomplete is cancelled (e.g. because the invoking
// frame was navigated to a different URL).
IPC_MESSAGE_ROUTED0(AutofillHostMsg_CancelRequestAutocomplete)

// Send when a text field is done editing.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_DidEndTextFieldEditing)

// Instructs the browser to hide the Autofill popup if it is open.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_HidePopup)

// Instructs the browser to show the password generation popup at the
// specified location. This location should be specified in the renderers
// coordinate system. Form is the form associated with the password field.
IPC_MESSAGE_ROUTED3(AutofillHostMsg_ShowPasswordGenerationPopup,
                    gfx::RectF /* source location */,
                    int /* max length of the password */,
                    autofill::PasswordForm)

// Instructs the browser to show the popup for editing a generated password.
// The location should be specified in the renderers coordinate system. Form
// is the form associated with the password field.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_ShowPasswordEditingPopup,
                    gfx::RectF /* source location */,
                    autofill::PasswordForm)

// Instructs the browser to hide any password generation popups.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_HidePasswordGenerationPopup)

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
