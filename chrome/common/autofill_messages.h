// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <string>

#include "chrome/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/password_form_dom_manager.h"

#define IPC_MESSAGE_START AutoFillMsgStart

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::FormField)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(form_control_type)
  IPC_STRUCT_TRAITS_MEMBER(max_length)
  IPC_STRUCT_TRAITS_MEMBER(is_autofilled)
  IPC_STRUCT_TRAITS_MEMBER(option_strings)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::FormData)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(user_submitted)
  IPC_STRUCT_TRAITS_MEMBER(fields)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::PasswordFormFillData)
  IPC_STRUCT_TRAITS_MEMBER(basic_data)
  IPC_STRUCT_TRAITS_MEMBER(additional_logins)
  IPC_STRUCT_TRAITS_MEMBER(wait_for_username)
IPC_STRUCT_TRAITS_END()

// AutoFill messages sent from the browser to the renderer.

// Reply to the AutoFillHostMsg_QueryFormFieldAutoFill message with the
// AutoFill suggestions.
IPC_MESSAGE_ROUTED5(AutoFillMsg_SuggestionsReturned,
                    int /* id of the request message */,
                    std::vector<string16> /* names */,
                    std::vector<string16> /* labels */,
                    std::vector<string16> /* icons */,
                    std::vector<int> /* unique_ids */)

// Reply to the AutoFillHostMsg_FillAutoFillFormData message with the
// AutoFill form data.
IPC_MESSAGE_ROUTED2(AutoFillMsg_FormDataFilled,
                    int /* id of the request message */,
                    webkit_glue::FormData /* form data */)

// Fill a password form and prepare field autocomplete for multiple
// matching logins.
IPC_MESSAGE_ROUTED1(AutoFillMsg_FillPasswordForm,
                    webkit_glue::PasswordFormFillData)


// AutoFill messages sent from the renderer to the browser.

// Notification that forms have been seen that are candidates for
// filling/submitting by the AutofillManager.
IPC_MESSAGE_ROUTED1(AutoFillHostMsg_FormsSeen,
                    std::vector<webkit_glue::FormData> /* forms */)

// Notification that password forms have been seen that are candidates for
// filling/submitting by the password manager.
IPC_MESSAGE_ROUTED1(AutoFillHostMsg_PasswordFormsFound,
                    std::vector<webkit_glue::PasswordForm> /* forms */)

// Notification that initial layout has occurred and the following password
// forms are visible on the page (e.g. not set to display:none.)
IPC_MESSAGE_ROUTED1(AutoFillHostMsg_PasswordFormsVisible,
                    std::vector<webkit_glue::PasswordForm> /* forms */)

// Notification that a form has been submitted.  The user hit the button.
IPC_MESSAGE_ROUTED1(AutoFillHostMsg_FormSubmitted,
                    webkit_glue::FormData /* form */)

// Queries the browser for AutoFill suggestions for a form input field.
IPC_MESSAGE_ROUTED3(AutoFillHostMsg_QueryFormFieldAutoFill,
                    int /* id of this message */,
                    webkit_glue::FormData /* the form */,
                    webkit_glue::FormField /* the form field */)

// Sent when the popup with AutoFill suggestions for a form is shown.
IPC_MESSAGE_ROUTED0(AutoFillHostMsg_DidShowAutoFillSuggestions)

// Instructs the browser to fill in the values for a form using AutoFill
// profile data.
IPC_MESSAGE_ROUTED4(AutoFillHostMsg_FillAutoFillFormData,
                    int /* id of this message */,
                    webkit_glue::FormData /* the form  */,
                    webkit_glue::FormField /* the form field  */,
                    int /* profile unique ID */)

// Sent when a form is previewed or filled with AutoFill suggestions.
IPC_MESSAGE_ROUTED0(AutoFillHostMsg_DidFillAutoFillFormData)

// Instructs the browser to remove the specified Autocomplete entry from the
// database.
IPC_MESSAGE_ROUTED2(AutoFillHostMsg_RemoveAutocompleteEntry,
                    string16 /* field name */,
                    string16 /* value */)

// Instructs the browser to show the AutoFill dialog.
IPC_MESSAGE_ROUTED0(AutoFillHostMsg_ShowAutoFillDialog)


