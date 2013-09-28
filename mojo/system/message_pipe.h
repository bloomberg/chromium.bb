// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MESSAGE_PIPE_H_
#define MOJO_SYSTEM_MESSAGE_PIPE_H_

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/public/system/core.h"
#include "mojo/system/waiter_list.h"

namespace mojo {
namespace system {

class Waiter;

// |MessagePipe| is the secondary object implementing a message pipe (see the
// explanatory comment in core_impl.cc), and is jointly owned by the two
// dispatchers passed in to the constructor. This class is thread-safe.
class MessagePipe : public base::RefCountedThreadSafe<MessagePipe> {
 public:
  MessagePipe();

  // These are called by the dispatcher to implement its methods of
  // corresponding names. In all cases, the port |port| must be open.
  void CancelAllWaiters(unsigned port);
  void Close(unsigned port);
  MojoResult WriteMessage(unsigned port,
                          const void* bytes, uint32_t num_bytes,
                          const MojoHandle* handles, uint32_t num_handles,
                          MojoWriteMessageFlags flags);
  MojoResult ReadMessage(unsigned port,
                         void* bytes, uint32_t* num_bytes,
                         MojoHandle* handles, uint32_t* num_handles,
                         MojoReadMessageFlags flags);
  MojoResult AddWaiter(unsigned port,
                       Waiter* waiter,
                       MojoWaitFlags flags,
                       MojoResult wake_result);
  void RemoveWaiter(unsigned port, Waiter* waiter);

 private:
  struct MessageInTransit {
    MessageInTransit(const void* bytes, uint32_t num_bytes)
        : data(static_cast<const char*>(bytes), num_bytes) {}

    // TODO(vtl): Replace with something more efficient.
    std::string data;
  };

  friend class base::RefCountedThreadSafe<MessagePipe>;
  virtual ~MessagePipe();

  MojoWaitFlags SatisfiedFlagsNoLock(unsigned port);
  MojoWaitFlags SatisfiableFlagsNoLock(unsigned port);

  base::Lock lock_;  // Protects the following members.
  bool is_open_[2];
  // These are *incoming* queues for their corresponding ports. It owns its
  // contents.
  // TODO(vtl): When possible (with C++11), convert the plain pointers to
  // scoped_ptr/unique_ptr. (Then we'll be able to use an |std::queue| instead
  // of an |std::list|.)
  std::list<MessageInTransit*> message_queues_[2];
  WaiterList waiter_lists_[2];

  DISALLOW_COPY_AND_ASSIGN(MessagePipe);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MESSAGE_PIPE_H_
