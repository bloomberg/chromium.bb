// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or multiply-included shared traits file depending upon circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file. Hence, provide an include guard symbol but no pragma once.
#ifndef CHROME_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#define CHROME_COMMON_COMMON_PARAM_TRAITS_MACROS_H_

#include "chrome/common/content_settings.h"
#include "ipc/ipc_message_macros.h"

IPC_ENUM_TRAITS(ContentSetting)
IPC_ENUM_TRAITS(ContentSettingsType)

#endif  // CHROME_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
