// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/content/common/autofill_param_traits_macros.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#define IPC_MESSAGE_START AutofillMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(autofill::FormFieldData::CheckStatus,
                          autofill::FormFieldData::CheckStatus::CHECKED)

IPC_ENUM_TRAITS_MAX_VALUE(autofill::FormFieldData::RoleAttribute,
                          autofill::FormFieldData::ROLE_ATTRIBUTE_OTHER)

IPC_ENUM_TRAITS_MAX_VALUE(base::i18n::TextDirection,
                          base::i18n::TEXT_DIRECTION_NUM_DIRECTIONS - 1)

IPC_STRUCT_TRAITS_BEGIN(autofill::FormFieldData)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(form_control_type)
  IPC_STRUCT_TRAITS_MEMBER(autocomplete_attribute)
  IPC_STRUCT_TRAITS_MEMBER(placeholder)
  IPC_STRUCT_TRAITS_MEMBER(role)
  IPC_STRUCT_TRAITS_MEMBER(max_length)
  IPC_STRUCT_TRAITS_MEMBER(is_autofilled)
  IPC_STRUCT_TRAITS_MEMBER(check_status)
  IPC_STRUCT_TRAITS_MEMBER(is_focusable)
  IPC_STRUCT_TRAITS_MEMBER(should_autocomplete)
  IPC_STRUCT_TRAITS_MEMBER(text_direction)
  IPC_STRUCT_TRAITS_MEMBER(option_values)
  IPC_STRUCT_TRAITS_MEMBER(option_contents)
  IPC_STRUCT_TRAITS_MEMBER(css_classes)
  IPC_STRUCT_TRAITS_MEMBER(properties_mask)
IPC_STRUCT_TRAITS_END()

// Instructs the browser that generation is available for this particular form.
// This is used for UMA stats.
IPC_MESSAGE_ROUTED1(AutofillHostMsg_GenerationAvailableForForm,
                    autofill::PasswordForm)

// Instructs the browser to show the password generation popup at the
// specified location. This location should be specified in the renderers
// coordinate system. Form is the form associated with the password field.
IPC_MESSAGE_ROUTED5(AutofillHostMsg_ShowPasswordGenerationPopup,
                    gfx::RectF /* source location */,
                    int /* max length of the password */,
                    base::string16, /* password field */
                    bool,           /* is manually triggered */
                    autofill::PasswordForm)

// Instructs the browser to show the popup for editing a generated password.
// The location should be specified in the renderers coordinate system. Form
// is the form associated with the password field.
IPC_MESSAGE_ROUTED2(AutofillHostMsg_ShowPasswordEditingPopup,
                    gfx::RectF /* source location */,
                    autofill::PasswordForm)

// Instructs the browser to hide any password generation popups.
IPC_MESSAGE_ROUTED0(AutofillHostMsg_HidePasswordGenerationPopup)
