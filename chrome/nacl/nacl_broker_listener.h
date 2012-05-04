// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_BROKER_LISTENER_H_
#define CHROME_NACL_NACL_BROKER_LISTENER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "chrome/common/nacl_types.h"
#include "ipc/ipc_channel.h"

// The BrokerThread class represents the thread that handles the messages from
// the browser process and starts NaCl loader processes.
class NaClBrokerListener : public IPC::Channel::Listener {
 public:
  NaClBrokerListener();
  ~NaClBrokerListener();

  void Listen();

  // IPC::Channel::Listener implementation.
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
