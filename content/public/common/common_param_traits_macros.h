// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file. Hence, provide include guard but no pragma once.
#ifndef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#define CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_

#include "content/public/common/console_message_level.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/security_style.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebReferrerPolicy.h"
#include "webkit/glue/resource_type.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS(content::ConsoleMessageLevel)
IPC_ENUM_TRAITS(content::PageTransition)
IPC_ENUM_TRAITS(content::SecurityStyle)
IPC_ENUM_TRAITS(ResourceType::Type)
IPC_ENUM_TRAITS(WebKit::WebReferrerPolicy)

#endif  // CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
