// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_MAILBOX_H_
#define GPU_COMMAND_BUFFER_MAILBOX_H_

#include "../../gpu_export.h"
#include "../common/types.h"

namespace gpu {

struct GPU_EXPORT Mailbox {
  Mailbox();
  bool IsZero() const;
  void SetName(const int8* name);
  int8 name[64];
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_MAILBOX_H_

