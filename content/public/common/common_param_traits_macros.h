// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#define CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_

#include "content/public/common/console_message_level.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/password_form.h"
#include "content/public/common/security_style.h"
#include "content/public/common/ssl_status.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebReferrerPolicy.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/plugins/webplugininfo.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS(content::ConsoleMessageLevel)
IPC_ENUM_TRAITS(content::PageTransition)
IPC_ENUM_TRAITS(content::SecurityStyle)
IPC_ENUM_TRAITS(WebKit::WebReferrerPolicy)
IPC_ENUM_TRAITS(WindowOpenDisposition)

IPC_STRUCT_TRAITS_BEGIN(WebKit::WebPoint)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebKit::WebRect)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::PasswordForm)
IPC_STRUCT_TRAITS_MEMBER(signon_realm)
IPC_STRUCT_TRAITS_MEMBER(origin)
IPC_STRUCT_TRAITS_MEMBER(action)
IPC_STRUCT_TRAITS_MEMBER(submit_element)
IPC_STRUCT_TRAITS_MEMBER(username_element)
IPC_STRUCT_TRAITS_MEMBER(username_value)
IPC_STRUCT_TRAITS_MEMBER(password_element)
IPC_STRUCT_TRAITS_MEMBER(password_value)
IPC_STRUCT_TRAITS_MEMBER(old_password_element)
IPC_STRUCT_TRAITS_MEMBER(old_password_value)
IPC_STRUCT_TRAITS_MEMBER(ssl_valid)
IPC_STRUCT_TRAITS_MEMBER(preferred)
IPC_STRUCT_TRAITS_MEMBER(blacklisted_by_user)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SSLStatus)
  IPC_STRUCT_TRAITS_MEMBER(security_style)
  IPC_STRUCT_TRAITS_MEMBER(cert_id)
  IPC_STRUCT_TRAITS_MEMBER(cert_status)
  IPC_STRUCT_TRAITS_MEMBER(security_bits)
  IPC_STRUCT_TRAITS_MEMBER(connection_status)
  IPC_STRUCT_TRAITS_MEMBER(content_status)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit::WebPluginMimeType)
  IPC_STRUCT_TRAITS_MEMBER(mime_type)
  IPC_STRUCT_TRAITS_MEMBER(file_extensions)
  IPC_STRUCT_TRAITS_MEMBER(description)
  IPC_STRUCT_TRAITS_MEMBER(additional_param_names)
  IPC_STRUCT_TRAITS_MEMBER(additional_param_values)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit::WebPluginInfo)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(path)
  IPC_STRUCT_TRAITS_MEMBER(version)
  IPC_STRUCT_TRAITS_MEMBER(desc)
  IPC_STRUCT_TRAITS_MEMBER(mime_types)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(pepper_permissions)
IPC_STRUCT_TRAITS_END()

#endif  // CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
