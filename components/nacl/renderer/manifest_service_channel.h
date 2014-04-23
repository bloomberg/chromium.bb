// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_MANIFEST_SERVICE_CHANNEL_H_
#define COMPONENTS_NACL_RENDERER_MANIFEST_SERVICE_CHANNEL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_listener.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace IPC {
struct ChannelHandle;
class Message;
class SyncChannel;
}  // namespace IPC

namespace nacl {

class ManifestServiceChannel : public IPC::Listener {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when PPAPI initialization in the NaCl plugin is finished.
    virtual void StartupInitializationComplete() = 0;

    // TODO(hidehiko): Add OpenResource() here.
  };

  ManifestServiceChannel(
      const IPC::ChannelHandle& handle,
      const base::Callback<void(int32_t)>& connected_callback,
      scoped_ptr<Delegate> delegate,
      base::WaitableEvent* waitable_event);
  virtual ~ManifestServiceChannel();

  // Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  void OnStartupInitializationComplete();

  base::Callback<void(int32_t)> connected_callback_;
  scoped_ptr<Delegate> delegate_;
  scoped_ptr<IPC::SyncChannel> channel_;

  DISALLOW_COPY_AND_ASSIGN(ManifestServiceChannel);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_MANIFEST_SERVICE_CHANNEL_H_
