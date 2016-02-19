// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_CHANNEL_INIT_H_
#define CONTENT_COMMON_MOJO_CHANNEL_INIT_H_

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace base {
class TaskRunner;
}

namespace content {

// ChannelInit handles creation and destruction of the Mojo channel. It is not
// thread-safe, but may be used on any single thread with a MessageLoop.
//
// TODO(rockot): Get rid of this class ASAP (i.e. once the patch which includes
// this TODO has stuck for a bit) since it's no longer necessary.
class CONTENT_EXPORT ChannelInit {
 public:
  ChannelInit();
  ~ChannelInit();

  // Initializes the channel. This takes ownership of |file|.
  mojo::ScopedMessagePipeHandle Init(
      base::PlatformFile file,
      scoped_refptr<base::TaskRunner> io_thread_task_runner);

 private:
  DISALLOW_COPY_AND_ASSIGN(ChannelInit);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_CHANNEL_INIT_H_
