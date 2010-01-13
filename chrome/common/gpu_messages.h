// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_MESSAGES_H_
#define CHROME_COMMON_GPU_MESSAGES_H_

#include <vector>

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/transport_dib.h"

namespace IPC {

// Potential new structures for messages go here.

}  // namespace IPC

#define MESSAGES_INTERNAL_FILE \
    "chrome/common/gpu_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_GPU_MESSAGES_H_

