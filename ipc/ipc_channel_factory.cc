// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "ipc/ipc_channel_factory.h"

namespace IPC {

namespace {

class PlatformChannelFactory : public ChannelFactory {
 public:
  PlatformChannelFactory(ChannelHandle handle, Channel::Mode mode)
      : handle_(handle), mode_(mode) {}

  std::string GetName() const override {
    return handle_.name;
  }

  scoped_ptr<Channel> BuildChannel(Listener* listener) override {
    return Channel::Create(handle_, mode_, listener);
  }

 private:
  ChannelHandle handle_;
  Channel::Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(PlatformChannelFactory);
};

} // namespace

// static
scoped_ptr<ChannelFactory> ChannelFactory::Create(const ChannelHandle& handle,
                                                  Channel::Mode mode) {
  return scoped_ptr<ChannelFactory>(new PlatformChannelFactory(handle, mode));
}

}  // namespace IPC
