// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_MANIFEST_SERVICE_CHANNEL_H_
#define COMPONENTS_NACL_RENDERER_MANIFEST_SERVICE_CHANNEL_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
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
  typedef base::Callback<void(base::File)> OpenResourceCallback;

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when PPAPI initialization in the NaCl plugin is finished.
    virtual void StartupInitializationComplete() = 0;

    // Called when irt_open_resource() is invoked in the NaCl plugin.
    // Upon completion, callback is invoked with the file.
    virtual void OpenResource(
        const std::string& key,
        const OpenResourceCallback& callback) = 0;
  };

  ManifestServiceChannel(
      const IPC::ChannelHandle& handle,
      const base::Callback<void(int32_t)>& connected_callback,
      scoped_ptr<Delegate> delegate,
      base::WaitableEvent* waitable_event);
  virtual ~ManifestServiceChannel();

  void Send(IPC::Message* message);

  // Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  void OnStartupInitializationComplete();
  void OnOpenResource(const std::string& key, IPC::Message* reply);
#if !defined(OS_WIN)
  void DidOpenResource(IPC::Message* reply, base::File file);
#endif

  base::Callback<void(int32_t)> connected_callback_;
  scoped_ptr<Delegate> delegate_;
  scoped_ptr<IPC::SyncChannel> channel_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ManifestServiceChannel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManifestServiceChannel);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_MANIFEST_SERVICE_CHANNEL_H_
