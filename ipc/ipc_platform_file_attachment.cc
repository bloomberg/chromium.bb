// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_platform_file_attachment.h"

namespace IPC {
namespace internal {

PlatformFileAttachment::PlatformFileAttachment(base::PlatformFile file)
    : file_(file) {
}

PlatformFileAttachment::~PlatformFileAttachment() {
}

MessageAttachment::Type PlatformFileAttachment::GetType() const {
  return TYPE_PLATFORM_FILE;
}

base::PlatformFile GetPlatformFile(
    scoped_refptr<MessageAttachment> attachment) {
  DCHECK_EQ(attachment->GetType(), MessageAttachment::TYPE_PLATFORM_FILE);
  return static_cast<PlatformFileAttachment*>(attachment.get())->file();
}

}  // namespace internal
}  // namespace IPC
