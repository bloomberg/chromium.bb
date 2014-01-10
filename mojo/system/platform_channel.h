// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_PLATFORM_CHANNEL_H_
#define MOJO_SYSTEM_PLATFORM_CHANNEL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/system/platform_channel_handle.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class MOJO_SYSTEM_IMPL_EXPORT PlatformChannel {
 public:
  virtual ~PlatformChannel();

  // Creates a channel if you already have the underlying handle for it, taking
  // ownership of |handle|.
  static scoped_ptr<PlatformChannel> CreateFromHandle(
      const PlatformChannelHandle& handle);

  // Returns the channel's handle, passing ownership.
  PlatformChannelHandle PassHandle();

  bool is_valid() const { return handle_.is_valid(); }

 protected:
  PlatformChannel();

  PlatformChannelHandle* mutable_handle() { return &handle_; }

 private:
  PlatformChannelHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(PlatformChannel);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_PLATFORM_CHANNEL_H_
