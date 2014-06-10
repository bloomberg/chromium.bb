// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_
#define COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_listener.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace IPC {
class ChannelProxy;
struct ChannelHandle;
class Message;
}  // namespace IPC

namespace nacl {

class TrustedPluginChannel : public IPC::Listener {
 public:
  explicit TrustedPluginChannel(const IPC::ChannelHandle& handle);
  virtual ~TrustedPluginChannel();

  bool Send(IPC::Message* message);

  // Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  scoped_ptr<IPC::ChannelProxy> channel_proxy_;

  DISALLOW_COPY_AND_ASSIGN(TrustedPluginChannel);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_
