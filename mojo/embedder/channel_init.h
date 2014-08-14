// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EMBEDDER_CHANNEL_INIT_H_
#define MOJO_EMBEDDER_CHANNEL_INIT_H_

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/system/system_impl_export.h"

namespace base {
class MessageLoopProxy;
class TaskRunner;
}

namespace mojo {
namespace embedder {
struct ChannelInfo;
}

namespace embedder {

// |ChannelInit| handles creation (and destruction) of the Mojo channel. It is
// not thread-safe, but may be used on any single thread (with a |MessageLoop|).
class MOJO_SYSTEM_IMPL_EXPORT ChannelInit {
 public:
  ChannelInit();
  ~ChannelInit();

  // Initializes the channel. This takes ownership of |file|. Returns the
  // primordial MessagePipe for the channel.
  mojo::ScopedMessagePipeHandle Init(
      base::PlatformFile file,
      scoped_refptr<base::TaskRunner> io_thread_task_runner);

  // Notifies the channel that we (hence it) will soon be destroyed.
  void WillDestroySoon();

 private:
  // Invoked on the thread on which this object lives once the channel has been
  // established. (This is a static method that takes a weak pointer to self,
  // since we want to destroy the channel even if we're destroyed.)
  static void OnCreatedChannel(base::WeakPtr<ChannelInit> self,
                               scoped_refptr<base::TaskRunner> io_thread,
                               embedder::ChannelInfo* channel);

  scoped_refptr<base::TaskRunner> io_thread_task_runner_;

  // If non-null the channel has been established.
  embedder::ChannelInfo* channel_info_;

  base::WeakPtrFactory<ChannelInit> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelInit);
};

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_EMBEDDER_CHANNEL_INIT_H_
