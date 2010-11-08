// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_MESSAGES_H_
#define CHROME_COMMON_GPU_MESSAGES_H_
#pragma once

#include "base/basictypes.h"
#include "base/process.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/gpu_param_traits.h"
#include "gfx/native_widget_types.h"
#include "gpu/command_buffer/common/command_buffer.h"

#define MESSAGES_INTERNAL_FILE \
    "chrome/common/gpu_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_GPU_MESSAGES_H_
