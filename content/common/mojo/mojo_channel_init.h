// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_MOJO_CHANNEL_INIT_H_
#define CONTENT_COMMON_MOJO_MOJO_CHANNEL_INIT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "content/common/content_export.h"
#include "mojo/public/system/core_cpp.h"

namespace base {
class MessageLoopProxy;
class TaskRunner;
}

namespace mojo {
namespace embedder {
struct ChannelInfo;
}
}

namespace content {

// MojoChannelInit handle creation (and destruction) of the mojo channel. It is
// expected that this class is created and destroyed on the main thread.
class CONTENT_EXPORT MojoChannelInit {
 public:
  MojoChannelInit();
  ~MojoChannelInit();

  // Inits the channel. This takes ownership of |file|.
  void Init(base::PlatformFile file,
            scoped_refptr<base::TaskRunner> io_thread_task_runner);

  bool is_handle_valid() const { return bootstrap_message_pipe_.is_valid(); }

 private:
  // Invoked on the IO thread once the channel has been established.
  static void OnCreatedChannelOnIOThread(
      base::WeakPtr<MojoChannelInit> host,
      scoped_refptr<base::TaskRunner> main_thread,
      scoped_refptr<base::TaskRunner> io_thread,
      mojo::embedder::ChannelInfo* channel);

  // Invoked on the main thread once the channel has been established.
  static void OnCreatedChannelOnMainThread(
      base::WeakPtr<MojoChannelInit> host,
      scoped_refptr<base::TaskRunner> io_thread,
      mojo::embedder::ChannelInfo* channel);

  // Thread where this object lives.
  scoped_refptr<base::TaskRunner> main_thread_;

  scoped_refptr<base::TaskRunner> io_thread_task_runner_;

  // If non-null the channel has been established.
  mojo::embedder::ChannelInfo* channel_info_;

  // The handle from channel creation.
  mojo::ScopedHandle bootstrap_message_pipe_;

  base::WeakPtrFactory<MojoChannelInit> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoChannelInit);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_MOJO_CHANNEL_INIT_H_
