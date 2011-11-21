// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_LISTENER_H_
#define CHROME_NACL_NACL_LISTENER_H_
#pragma once

#include <vector>

#include "chrome/common/nacl_types.h"
#include "ipc/ipc_channel.h"

// The NaClListener is an IPC channel listener that waits for a
// request to start a NaCl module.
class NaClListener : public IPC::Channel::Listener {
 public:
  NaClListener();
  virtual ~NaClListener();
  // Listen for a request to launch a NaCl module.
  void Listen();
  void set_debug_enabled(bool value) {debug_enabled_ = value;}

 private:
  void OnStartSelLdr(std::vector<nacl::FileDescriptor> handles);
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  bool debug_enabled_;

  DISALLOW_COPY_AND_ASSIGN(NaClListener);
};

#endif  // CHROME_NACL_NACL_LISTENER_H_
