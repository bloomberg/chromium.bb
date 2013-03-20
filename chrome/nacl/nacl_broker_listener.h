// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_BROKER_LISTENER_H_
#define CHROME_NACL_NACL_BROKER_LISTENER_H_

#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "chrome/common/nacl_types.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Channel;
}

// The BrokerThread class represents the thread that handles the messages from
// the browser process and starts NaCl loader processes.
class NaClBrokerListener : public content::SandboxedProcessLauncherDelegate,
                           public IPC::Listener {
 public:
  NaClBrokerListener();
  ~NaClBrokerListener();

  void Listen();

  // content::SandboxedProcessLauncherDelegate implementation:
  virtual void PreSpawnTarget(sandbox::TargetPolicy* policy,
                              bool* success) OVERRIDE;

  // IPC::Listener implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  void OnLaunchLoaderThroughBroker(const std::string& loader_channel_id);
  void OnLaunchDebugExceptionHandler(int32 pid,
                                     base::ProcessHandle process_handle,
                                     const std::string& startup_info);
  void OnStopBroker();

  base::ProcessHandle browser_handle_;
  scoped_ptr<IPC::Channel> channel_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrokerListener);
};

#endif  // CHROME_NACL_NACL_BROKER_LISTENER_H_
