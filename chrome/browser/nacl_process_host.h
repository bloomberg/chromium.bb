// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_PROCESS_HOST_H_
#define CHROME_BROWSER_NACL_PROCESS_HOST_H_

#include "build/build_config.h"

#include "base/ref_counted.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/nacl_types.h"
#include "native_client/src/shared/imc/nacl_imc.h"

class ResourceMessageFilter;

// Represents the browser side of the browser <--> NaCl communication
// channel. There will be one NaClProcessHost per NaCl process
// The browser is responsible for starting the NaCl process
// when requested by the renderer.
// After that, most of the communication is directly between NaCl plugin
// running in the renderer and NaCl processes.
class NaClProcessHost : public ChildProcessHost {
 public:
  NaClProcessHost(ResourceDispatcherHost *resource_dispatcher_host,
                  const std::wstring& url);
  ~NaClProcessHost();

  // Initialize the new NaCl process, returning true on success.
  bool Launch(ResourceMessageFilter* resource_message_filter,
              const int descriptor,
              IPC::Message* reply_msg);

  virtual void OnMessageReceived(const IPC::Message& msg);

  void OnProcessLaunchedByBroker(base::ProcessHandle handle);

 protected:
  virtual bool DidChildCrash();

 private:
  bool LaunchSelLdr();

  void SendStartMessage();

  virtual void OnProcessLaunched();

  // ResourceDispatcherHost::Receiver implementation:
  virtual URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data);

  virtual bool CanShutdown() { return true; }

#if defined(OS_WIN)
  // Check whether the browser process is running on WOW64 - Windows only
  void CheckIsWow64();
#endif

 private:
  ResourceDispatcherHost* resource_dispatcher_host_;

  // The ResourceMessageFilter that requested this NaCl process.  We use this
  // for sending the reply once the process has started.
  scoped_refptr<ResourceMessageFilter> resource_message_filter_;

  // The reply message to send.
  IPC::Message* reply_msg_;

  // The socket pair for the NaCl process.
  nacl::Handle pair_[2];

  // The NaCl specific descriptor for this process.
  int descriptor_;

  // Windows platform flag
  bool running_on_wow64_;

  DISALLOW_COPY_AND_ASSIGN(NaClProcessHost);
};

#endif  // CHROME_BROWSER_NACL_PROCESS_HOST_H_
