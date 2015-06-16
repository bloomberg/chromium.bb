// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_factory.h"

namespace IPC {

namespace {

class PlatformChannelFactory : public ChannelFactory {
 public:
  PlatformChannelFactory(ChannelHandle handle,
                         Channel::Mode mode,
                         AttachmentBroker* broker)
      : handle_(handle), mode_(mode), broker_(broker) {}

  std::string GetName() const override {
    return handle_.name;
  }

  scoped_ptr<Channel> BuildChannel(Listener* listener) override {
    return Channel::Create(handle_, mode_, listener, broker_);
  }

 private:
  ChannelHandle handle_;
  Channel::Mode mode_;
  AttachmentBroker* broker_;

  DISALLOW_COPY_AND_ASSIGN(PlatformChannelFactory);
};

} // namespace

// static
scoped_ptr<ChannelFactory> ChannelFactory::Create(const ChannelHandle& handle,
                                                  Channel::Mode mode,
                                                  AttachmentBroker* broker) {
  return scoped_ptr<ChannelFactory>(
      new PlatformChannelFactory(handle, mode, broker));
}

}  // namespace IPC
