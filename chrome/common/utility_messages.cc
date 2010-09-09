// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/utility_messages.h"

#include "base/values.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/serialized_script_value.h"
#include "base/file_path.h"
#include "third_party/skia/include/core/SkBitmap.h"

#define MESSAGES_INTERNAL_IMPL_FILE \
  "chrome/common/utility_messages_internal.h"
#include "ipc/ipc_message_impl_macros.h"
