// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_MAILBOX_H_
#define GPU_COMMAND_BUFFER_MAILBOX_H_

#include <string.h>

#include "gpu/command_buffer/common/types.h"
#include "gpu/gpu_export.h"

// From gl2/gl2ext.h.
#ifndef GL_MAILBOX_SIZE_CHROMIUM
#define GL_MAILBOX_SIZE_CHROMIUM 64
#endif

namespace gpu {

struct GPU_EXPORT Mailbox {
  Mailbox();
  bool IsZero() const;
  void SetZero();
  void SetName(const int8* name);
  int8 name[GL_MAILBOX_SIZE_CHROMIUM];
  bool operator<(const Mailbox& other) const {
    return memcmp(this, &other, sizeof other) < 0;
  }
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_MAILBOX_H_

