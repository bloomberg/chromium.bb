// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_AUTOFILL_MESSAGES_H_
#define CHROME_COMMON_AUTOFILL_MESSAGES_H_
#pragma once

#include <string>

#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START AutoFillMsgStart

namespace webkit_glue {
class FormField;
struct FormData;
struct PasswordForm;
struct PasswordFormFillData;
}

namespace IPC {

template <>
struct ParamTraits<webkit_glue::FormField> {
  typedef webkit_glue::FormField param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};


// Traits for FormData structure to pack/unpack.
template <>
struct ParamTraits<webkit_glue::FormData> {
  typedef webkit_glue::FormData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webkit_glue::PasswordFormFillData> {
  typedef webkit_glue::PasswordFormFillData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

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

#endif  // CHROME_COMMON_AUTOFILL_MESSAGES_H_
