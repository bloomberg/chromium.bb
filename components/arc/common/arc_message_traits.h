// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/arc_message_types.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

IPC_ENUM_TRAITS_MIN_MAX_VALUE(arc::ScaleFactor,
                              arc::ScaleFactor::SCALE_FACTOR_100P,
                              arc::ScaleFactor::NUM_SCALE_FACTORS);

IPC_STRUCT_TRAITS_BEGIN(arc::AppInfo)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(package)
  IPC_STRUCT_TRAITS_MEMBER(activity)
IPC_STRUCT_TRAITS_END()
