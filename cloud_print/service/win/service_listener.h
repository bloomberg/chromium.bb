// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_SERVICE_SERVICE_LISTENER_H_
#define CLOUD_PRINT_SERVICE_SERVICE_LISTENER_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_listener.h"

namespace base {
class Thread;
}  // base

namespace IPC {
class Channel;
}  // IPC

// Simple IPC listener to run on service side to collect service environment and
// to send back to setup utility.
class ServiceListener : public IPC::Listener {
 public:
  explicit ServiceListener(const base::FilePath& user_data_dir);
  virtual ~ServiceListener();

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

 private:
  void Disconnect();
  void Connect();

  scoped_ptr<base::Thread> ipc_thread_;
  scoped_ptr<IPC::Channel> channel_;
  base::FilePath user_data_dir_;
};

#endif  // CLOUD_PRINT_SERVICE_SERVICE_LISTENER_H_

