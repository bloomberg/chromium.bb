// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_BROKER_HOST_WIN_H_
#define CHROME_BROWSER_NACL_HOST_NACL_BROKER_HOST_WIN_H_
#pragma once

#include "base/basictypes.h"
#include "content/browser/browser_child_process_host.h"

class NaClBrokerHost : public BrowserChildProcessHost {
 public:
  explicit NaClBrokerHost(ResourceDispatcherHost* resource_dispatcher_host);
  ~NaClBrokerHost();

  // This function starts the broker process. It needs to be called
  // before loaders can be launched.
  bool Init();

  // Send a message to the broker process, causing it to launch
  // a Native Client loader process.
  bool LaunchLoader(const std::wstring& loader_channel_id);

  // Stop the broker process.
  void StopBroker();

 private:
  virtual bool CanShutdown() { return true; }

  // Handler for NaClProcessMsg_LoaderLaunched message
  void OnLoaderLaunched(const std::wstring& loader_channel_id,
                        base::ProcessHandle handle);

  // IPC::Channel::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg);

  bool stopping_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrokerHost);
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_BROKER_HOST_WIN_H_
