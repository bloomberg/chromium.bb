// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_PLATFORM_FILE_ATTACHMENT_H_
#define IPC_IPC_PLATFORM_FILE_ATTACHMENT_H_

#include "base/files/file.h"
#include "ipc/ipc_message_attachment.h"

namespace IPC {
namespace internal {

// A platform file that is sent over |Channel| as a part of |Message|.
// |PlatformFileAttachment| doesn't own |file_|. The lifecycle of |file_| is
// managed by |MessageAttachmentSet|.
class PlatformFileAttachment : public MessageAttachment {
 public:
  explicit PlatformFileAttachment(base::PlatformFile file);

  Type GetType() const override;
  base::PlatformFile file() const { return file_; }

 private:
  ~PlatformFileAttachment() override;

  base::PlatformFile file_;
};

base::PlatformFile GetPlatformFile(scoped_refptr<MessageAttachment> attachment);

}  // namespace internal
}  // namespace IPC

#endif  // IPC_IPC_PLATFORM_FILE_ATTACHMENT_H_
