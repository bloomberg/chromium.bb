// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_attachment_set.h"

#include <algorithm>
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_platform_file_attachment.h"

#if defined(OS_POSIX)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif // OS_POSIX

namespace IPC {

MessageAttachmentSet::MessageAttachmentSet()
    : consumed_descriptor_highwater_(0) {
}

MessageAttachmentSet::~MessageAttachmentSet() {
  if (consumed_descriptor_highwater_ == size())
    return;

  // We close all the owning descriptors. If this message should have
  // been transmitted, then closing those with close flags set mirrors
  // the expected behaviour.
  //
  // If this message was received with more descriptors than expected
  // (which could a DOS against the browser by a rogue renderer) then all
  // the descriptors have their close flag set and we free all the extra
  // kernel resources.
  LOG(WARNING) << "MessageAttachmentSet destroyed with unconsumed descriptors: "
               << consumed_descriptor_highwater_ << "/" << size();
}

unsigned MessageAttachmentSet::num_descriptors() const {
  return std::count_if(attachments_.begin(), attachments_.end(),
                       [](scoped_refptr<MessageAttachment> i) {
    return i->GetType() == MessageAttachment::TYPE_PLATFORM_FILE;
  });
}

unsigned MessageAttachmentSet::size() const {
  return static_cast<unsigned>(attachments_.size());
}

void MessageAttachmentSet::AddAttachment(
    scoped_refptr<MessageAttachment> attachment) {
  attachments_.push_back(attachment);
}

scoped_refptr<MessageAttachment> MessageAttachmentSet::GetAttachmentAt(
    unsigned index) {
  if (index >= size()) {
    DLOG(WARNING) << "Accessing out of bound index:" << index << "/" << size();
    return scoped_refptr<MessageAttachment>();
  }

  // We should always walk the descriptors in order, so it's reasonable to
  // enforce this. Consider the case where a compromised renderer sends us
  // the following message:
  //
  //   ExampleMsg:
  //     num_fds:2 msg:FD(index = 1) control:SCM_RIGHTS {n, m}
  //
  // Here the renderer sent us a message which should have a descriptor, but
  // actually sent two in an attempt to fill our fd table and kill us. By
  // setting the index of the descriptor in the message to 1 (it should be
  // 0), we would record a highwater of 1 and then consider all the
  // descriptors to have been used.
  //
  // So we can either track of the use of each descriptor in a bitset, or we
  // can enforce that we walk the indexes strictly in order.
  //
  // There's one more wrinkle: When logging messages, we may reparse them. So
  // we have an exception: When the consumed_descriptor_highwater_ is at the
  // end of the array and index 0 is requested, we reset the highwater value.
  // TODO(morrita): This is absurd. This "wringle" disallow to introduce clearer
  // ownership model. Only client is NaclIPCAdapter. See crbug.com/415294
  if (index == 0 && consumed_descriptor_highwater_ == size())
    consumed_descriptor_highwater_ = 0;

  if (index != consumed_descriptor_highwater_)
    return scoped_refptr<MessageAttachment>();

  consumed_descriptor_highwater_ = index + 1;

  return attachments_[index];
}

#if defined(OS_POSIX)

bool MessageAttachmentSet::AddToBorrow(base::PlatformFile fd) {
  DCHECK_EQ(consumed_descriptor_highwater_, 0u);

  if (num_descriptors() == kMaxDescriptorsPerMessage) {
    DLOG(WARNING) << "Cannot add file descriptor. MessageAttachmentSet full.";
    return false;
  }

  AddAttachment(new internal::PlatformFileAttachment(fd));
  return true;
}

bool MessageAttachmentSet::AddToOwn(base::ScopedFD fd) {
  DCHECK_EQ(consumed_descriptor_highwater_, 0u);

  if (num_descriptors() == kMaxDescriptorsPerMessage) {
    DLOG(WARNING) << "Cannot add file descriptor. MessageAttachmentSet full.";
    return false;
  }

  AddAttachment(new internal::PlatformFileAttachment(fd.get()));
  owned_descriptors_.push_back(new base::ScopedFD(fd.Pass()));
  DCHECK(num_descriptors() <= kMaxDescriptorsPerMessage);
  return true;
}

base::PlatformFile MessageAttachmentSet::TakeDescriptorAt(unsigned index) {
  scoped_refptr<MessageAttachment> attachment = GetAttachmentAt(index);
  if (!attachment)
    return -1;

  base::PlatformFile file = internal::GetPlatformFile(attachment);

  // TODO(morrita): In production, attachments_.size() should be same as
  // owned_descriptors_.size() as all read descriptors are owned by Message.
  // We have to do this because unit test breaks this assumption. It should be
  // changed to exercise with own-able descriptors.
  for (ScopedVector<base::ScopedFD>::const_iterator i =
           owned_descriptors_.begin();
       i != owned_descriptors_.end(); ++i) {
    if ((*i)->get() == file) {
      ignore_result((*i)->release());
      break;
    }
  }

  return file;
}

void MessageAttachmentSet::PeekDescriptors(base::PlatformFile* buffer) const {
  for (size_t i = 0; i != attachments_.size(); ++i)
    buffer[i] = internal::GetPlatformFile(attachments_[i]);
}

bool MessageAttachmentSet::ContainsDirectoryDescriptor() const {
  struct stat st;

  for (auto i = attachments_.begin(); i != attachments_.end(); ++i) {
    if (fstat(internal::GetPlatformFile(*i), &st) == 0 && S_ISDIR(st.st_mode))
      return true;
  }

  return false;
}

void MessageAttachmentSet::CommitAll() {
  attachments_.clear();
  owned_descriptors_.clear();
  consumed_descriptor_highwater_ = 0;
}

void MessageAttachmentSet::ReleaseFDsToClose(
    std::vector<base::PlatformFile>* fds) {
  for (ScopedVector<base::ScopedFD>::iterator i = owned_descriptors_.begin();
       i != owned_descriptors_.end(); ++i) {
    fds->push_back((*i)->release());
  }

  CommitAll();
}

void MessageAttachmentSet::AddDescriptorsToOwn(const base::PlatformFile* buffer,
                                               unsigned count) {
  DCHECK(count <= kMaxDescriptorsPerMessage);
  DCHECK_EQ(num_descriptors(), 0u);
  DCHECK_EQ(consumed_descriptor_highwater_, 0u);

  attachments_.reserve(count);
  owned_descriptors_.reserve(count);
  for (unsigned i = 0; i < count; ++i) {
    AddAttachment(new internal::PlatformFileAttachment(buffer[i]));
    owned_descriptors_.push_back(new base::ScopedFD(buffer[i]));
  }
}

#endif  // OS_POSIX

}  // namespace IPC


