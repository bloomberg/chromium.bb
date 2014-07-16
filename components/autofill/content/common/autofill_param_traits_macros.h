// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or multiply-included shared traits file depending on circumstances.
// This allows the use of Autofill IPC serialization macros in more than one IPC
// message file.
#ifndef COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_PARAM_TRAITS_MACROS_H_
#define COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_PARAM_TRAITS_MACROS_H_

#include "components/autofill/core/common/password_form.h"
#include "ipc/ipc_message_macros.h"

IPC_ENUM_TRAITS_MAX_VALUE(autofill::PasswordForm::Type,
                          autofill::PasswordForm::TYPE_LAST)

IPC_STRUCT_TRAITS_BEGIN(autofill::FormData)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(user_submitted)
  IPC_STRUCT_TRAITS_MEMBER(fields)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(autofill::PasswordForm::Scheme,
                          autofill::PasswordForm::SCHEME_LAST)

IPC_STRUCT_TRAITS_BEGIN(autofill::PasswordForm)
  IPC_STRUCT_TRAITS_MEMBER(scheme)
  IPC_STRUCT_TRAITS_MEMBER(signon_realm)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(submit_element)
  IPC_STRUCT_TRAITS_MEMBER(username_element)
  IPC_STRUCT_TRAITS_MEMBER(username_value)
  IPC_STRUCT_TRAITS_MEMBER(other_possible_usernames)
  IPC_STRUCT_TRAITS_MEMBER(password_element)
  IPC_STRUCT_TRAITS_MEMBER(password_value)
  IPC_STRUCT_TRAITS_MEMBER(password_autocomplete_set)
  IPC_STRUCT_TRAITS_MEMBER(new_password_element)
  IPC_STRUCT_TRAITS_MEMBER(new_password_value)
  IPC_STRUCT_TRAITS_MEMBER(ssl_valid)
  IPC_STRUCT_TRAITS_MEMBER(preferred)
  IPC_STRUCT_TRAITS_MEMBER(blacklisted_by_user)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(times_used)
  IPC_STRUCT_TRAITS_MEMBER(form_data)
  // Excluding |use_additional_authentication| as it is only set and read in the
  // browser process and the renderer need not know about it.
IPC_STRUCT_TRAITS_END()

#endif  // COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_PARAM_TRAITS_MACROS_H_
