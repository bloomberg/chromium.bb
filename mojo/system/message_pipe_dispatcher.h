// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_
#define MOJO_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class MessagePipe;

// This is the |Dispatcher| implementation for message pipes (created by the
// Mojo primitive |MojoCreateMessagePipe()|). This class is thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT MessagePipeDispatcher : public Dispatcher {
 public:
  MessagePipeDispatcher();

  // Must be called before any other methods. (This method is not thread-safe.)
  void Init(scoped_refptr<MessagePipe> message_pipe, unsigned port);

 private:
  friend class base::RefCountedThreadSafe<MessagePipeDispatcher>;
  virtual ~MessagePipeDispatcher();

  // |Dispatcher| implementation/overrides:
  virtual void CancelAllWaitersNoLock() OVERRIDE;
  virtual MojoResult CloseImplNoLock() OVERRIDE;
  virtual MojoResult WriteMessageImplNoLock(
      const void* bytes, uint32_t num_bytes,
      const std::vector<Dispatcher*>* dispatchers,
      MojoWriteMessageFlags flags) OVERRIDE;
  virtual MojoResult ReadMessageImplNoLock(
      void* bytes, uint32_t* num_bytes,
      std::vector<scoped_refptr<Dispatcher> >* dispatchers,
      uint32_t* num_dispatchers,
      MojoReadMessageFlags flags) OVERRIDE;
  virtual MojoResult AddWaiterImplNoLock(Waiter* waiter,
                                         MojoWaitFlags flags,
                                         MojoResult wake_result) OVERRIDE;
  virtual void RemoveWaiterImplNoLock(Waiter* waiter) OVERRIDE;
  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() OVERRIDE;

  // Protected by |lock()|:
  scoped_refptr<MessagePipe> message_pipe_;  // This will be null if closed.
  unsigned port_;

  DISALLOW_COPY_AND_ASSIGN(MessagePipeDispatcher);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_
