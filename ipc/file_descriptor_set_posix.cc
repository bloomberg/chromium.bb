// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/file_descriptor_set_posix.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

FileDescriptorSet::FileDescriptorSet()
    : consumed_descriptor_highwater_(0) {
}

FileDescriptorSet::~FileDescriptorSet() {
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
  LOG(WARNING) << "FileDescriptorSet destroyed with unconsumed descriptors: "
               << consumed_descriptor_highwater_ << "/" << size();
}

bool FileDescriptorSet::AddToBorrow(base::PlatformFile fd) {
  DCHECK_EQ(consumed_descriptor_highwater_, 0u);

  if (size() == kMaxDescriptorsPerMessage) {
    DLOG(WARNING) << "Cannot add file descriptor. FileDescriptorSet full.";
    return false;
  }

  descriptors_.push_back(fd);
  return true;
}

bool FileDescriptorSet::AddToOwn(base::ScopedFD fd) {
  DCHECK_EQ(consumed_descriptor_highwater_, 0u);

  if (size() == kMaxDescriptorsPerMessage) {
    DLOG(WARNING) << "Cannot add file descriptor. FileDescriptorSet full.";
    return false;
  }

  descriptors_.push_back(fd.get());
  owned_descriptors_.push_back(new base::ScopedFD(fd.Pass()));
  DCHECK(size() <= kMaxDescriptorsPerMessage);
  return true;
}

base::PlatformFile FileDescriptorSet::TakeDescriptorAt(unsigned index) {
  if (index >= size()) {
    DLOG(WARNING) << "Accessing out of bound index:"
                  << index << "/" << size();
    return -1;
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
  if (index == 0 && consumed_descriptor_highwater_ == descriptors_.size())
    consumed_descriptor_highwater_ = 0;

  if (index != consumed_descriptor_highwater_)
    return -1;

  consumed_descriptor_highwater_ = index + 1;

  base::PlatformFile file = descriptors_[index];

  // TODO(morrita): In production, descriptors_.size() should be same as
  // owned_descriptors_.size() as all read descriptors are owned by Message.
  // We have to do this because unit test breaks this assumption. It should be
  // changed to exercise with own-able descriptors.
  for (ScopedVector<base::ScopedFD>::const_iterator i =
           owned_descriptors_.begin();
       i != owned_descriptors_.end();
       ++i) {
    if ((*i)->get() == file) {
      ignore_result((*i)->release());
      break;
    }
  }

  return file;
}

void FileDescriptorSet::PeekDescriptors(base::PlatformFile* buffer) const {
  std::copy(descriptors_.begin(), descriptors_.end(), buffer);
}

bool FileDescriptorSet::ContainsDirectoryDescriptor() const {
  struct stat st;

  for (std::vector<base::PlatformFile>::const_iterator i = descriptors_.begin();
       i != descriptors_.end();
       ++i) {
    if (fstat(*i, &st) == 0 && S_ISDIR(st.st_mode))
      return true;
  }

  return false;
}

void FileDescriptorSet::CommitAll() {
  descriptors_.clear();
  owned_descriptors_.clear();
  consumed_descriptor_highwater_ = 0;
}

void FileDescriptorSet::ReleaseFDsToClose(
    std::vector<base::PlatformFile>* fds) {
  for (ScopedVector<base::ScopedFD>::iterator i = owned_descriptors_.begin();
       i != owned_descriptors_.end();
       ++i) {
    fds->push_back((*i)->release());
  }

  CommitAll();
}

void FileDescriptorSet::AddDescriptorsToOwn(const base::PlatformFile* buffer,
                                            unsigned count) {
  DCHECK(count <= kMaxDescriptorsPerMessage);
  DCHECK_EQ(size(), 0u);
  DCHECK_EQ(consumed_descriptor_highwater_, 0u);

  descriptors_.reserve(count);
  owned_descriptors_.reserve(count);
  for (unsigned i = 0; i < count; ++i) {
    descriptors_.push_back(buffer[i]);
    owned_descriptors_.push_back(new base::ScopedFD(buffer[i]));
  }
}
