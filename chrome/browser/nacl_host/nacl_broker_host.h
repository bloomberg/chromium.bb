// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_BROKER_HOST_H_
#define CHROME_BROWSER_NACL_HOST_NACL_BROKER_HOST_H_

#include "base/basictypes.h"
#include "base/process.h"
#include "chrome/common/child_process_host.h"
#include "ipc/ipc_message.h"

class NaClBrokerHost : public ChildProcessHost {
 public:
  explicit NaClBrokerHost(ResourceDispatcherHost* resource_dispatcher_host);
  ~NaClBrokerHost();

  // This function starts the broker process. It needs to be called
  // before loaders can be launched.
  bool Init();

  // Send a message to the broker process, causing it to launch
  // a Native Client loader process.
  bool LaunchLoader(const std::wstring& loader_channel_id);

 private:
  // ResourceDispatcherHost::Receiver implementation:
  virtual URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data);

  virtual bool CanShutdown() { return true; }

  // Handler for NaClProcessMsg_BrokerReady message (sent by the broker process)
  void OnBrokerReady();
  // Handler for NaClProcessMsg_LoaderLaunched message
  void OnLoaderLaunched(const std::wstring& loader_channel_id,
                        base::ProcessHandle handle);

  // IPC::Channel::Listener
  virtual void OnMessageReceived(const IPC::Message& msg);

  DISALLOW_COPY_AND_ASSIGN(NaClBrokerHost);
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_BROKER_HOST_H_
