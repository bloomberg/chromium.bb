// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_PROCESS_HOST_H_
#define CHROME_BROWSER_NACL_PROCESS_HOST_H_

#include "build/build_config.h"
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
  ~NaClProcessHost() {}

  // Initialize the new NaCl process, returning true on success.
  bool Launch(ResourceMessageFilter* renderer_msg_filter,
              const int descriptor,
              nacl::FileDescriptor* handle,
              nacl::FileDescriptor* nacl_process_handle,
              int* nacl_process_id);

  virtual void OnMessageReceived(const IPC::Message& msg);

 private:
  bool LaunchSelLdr(ResourceMessageFilter* renderer_msg_filter,
                    const int descriptor,
                    const nacl::Handle handle);

  bool SendStartMessage(base::ProcessHandle process,
                        int descriptor,
                        nacl::Handle handle);

  // ResourceDispatcherHost::Receiver implementation:
  virtual URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data);

  virtual bool CanShutdown() { return true; }

 private:
  ResourceDispatcherHost* resource_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(NaClProcessHost);
};

#endif  // CHROME_BROWSER_NACL_PROCESS_HOST_H_
