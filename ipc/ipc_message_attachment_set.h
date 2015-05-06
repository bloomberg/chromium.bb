// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_ATTACHMENT_SET_H_
#define IPC_IPC_MESSAGE_ATTACHMENT_SET_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "ipc/ipc_export.h"

#if defined(OS_POSIX)
#include "base/files/file.h"
#endif

namespace IPC {

class MessageAttachment;

// -----------------------------------------------------------------------------
// A MessageAttachmentSet is an ordered set of MessageAttachment objects. These
// are associated with IPC messages so that attachments, each of which is either
// a platform file or a mojo handle, can be transmitted over the underlying UNIX
// domain socket (for ChannelPosix) or Mojo MessagePipe (for ChannelMojo).
// -----------------------------------------------------------------------------
class IPC_EXPORT MessageAttachmentSet
    : public base::RefCountedThreadSafe<MessageAttachmentSet> {
 public:
  MessageAttachmentSet();

  // Return the number of attachments
  unsigned size() const;
  // Return the number of file descriptors
  unsigned num_descriptors() const;
  // Return the number of mojo handles in the attachment set
  unsigned num_mojo_handles() const;

  // Return true if no unconsumed descriptors remain
  bool empty() const { return 0 == size(); }

  bool AddAttachment(scoped_refptr<MessageAttachment> attachment);

  // Take the nth attachment from the beginning of the set, Code using this
  // /must/ access the attachments in order, and must do it at most once.
  //
  // This interface is designed for the deserialising code as it doesn't
  // support close flags.
  //   returns: an attachment, or nullptr on error
  scoped_refptr<MessageAttachment> GetAttachmentAt(unsigned index);

  // This must be called after transmitting the descriptors returned by
  // PeekDescriptors. It marks all the descriptors as consumed and closes those
  // which are auto-close.
  void CommitAll();

#if defined(OS_POSIX)
  // This is the maximum number of descriptors per message. We need to know this
  // because the control message kernel interface has to be given a buffer which
  // is large enough to store all the descriptor numbers. Otherwise the kernel
  // tells us that it truncated the control data and the extra descriptors are
  // lost.
  //
  // In debugging mode, it's a fatal error to try and add more than this number
  // of descriptors to a MessageAttachmentSet.
  static const size_t kMaxDescriptorsPerMessage = 7;

  // ---------------------------------------------------------------------------
  // Interfaces for transmission...

  // Fill an array with file descriptors without 'consuming' them. CommitAll
  // must be called after these descriptors have been transmitted.
  //   buffer: (output) a buffer of, at least, size() integers.
  void PeekDescriptors(base::PlatformFile* buffer) const;
  // Returns true if any contained file descriptors appear to be handles to a
  // directory.
  bool ContainsDirectoryDescriptor() const;
  // Fetch all filedescriptors with the "auto close" property.
  // Used instead of CommitAll() when closing must be handled manually.
  void ReleaseFDsToClose(std::vector<base::PlatformFile>* fds);

  // ---------------------------------------------------------------------------

  // ---------------------------------------------------------------------------
  // Interfaces for receiving...

  // Set the contents of the set from the given buffer. This set must be empty
  // before calling. The auto-close flag is set on all the descriptors so that
  // unconsumed descriptors are closed on destruction.
  void AddDescriptorsToOwn(const base::PlatformFile* buffer, unsigned count);

#endif  // OS_POSIX

  // ---------------------------------------------------------------------------

 private:
  friend class base::RefCountedThreadSafe<MessageAttachmentSet>;

  ~MessageAttachmentSet();

  // A vector of attachments of the message, which might be |PlatformFile| or
  // |MessagePipe|.
  std::vector<scoped_refptr<MessageAttachment>> attachments_;

  // This contains the index of the next descriptor which should be consumed.
  // It's used in a couple of ways. Firstly, at destruction we can check that
  // all the descriptors have been read (with GetNthDescriptor). Secondly, we
  // can check that they are read in order.
  mutable unsigned consumed_descriptor_highwater_;

  DISALLOW_COPY_AND_ASSIGN(MessageAttachmentSet);
};

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_ATTACHMENT_SET_H_
