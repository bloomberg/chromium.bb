// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_CHILD_CONNECTION_H_
#define CONTENT_BROWSER_MOJO_MOJO_CHILD_CONNECTION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequenced_task_runner.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "services/shell/public/interfaces/connector.mojom.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/service_registry_android.h"
#endif

namespace shell {
class Connection;
class Connector;
}

namespace content {

// Helper class to establish a connection between the shell and a single child
// process. Process hosts can use this when launching new processes which
// should be registered with the shell.
class MojoChildConnection {
 public:
  // Prepares a new child connection for a child process which will be
  // identified to the shell as |application_name|. |instance_id| must be
  // unique among all child connections using the same |application_name|.
  // |connector| is the connector to use to establish the connection.
  MojoChildConnection(const std::string& application_name,
                      const std::string& instance_id,
                      const std::string& child_token,
                      shell::Connector* connector,
                      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~MojoChildConnection();

  shell::InterfaceRegistry* GetInterfaceRegistry() {
    return &interface_registry_;
  }

  shell::InterfaceProvider* GetRemoteInterfaces() {
    return &remote_interfaces_;
  }

  // A token which must be passed to the child process via
  // |switches::kPrimordialPipeToken| in order for the child to initialize its
  // end of the shell connection pipe.
  std::string service_token() const { return service_token_; }

  // Sets the child connection's process handle. This should be called as soon
  // as the process has been launched, and the connection will not be fully
  // functional until this is called.
  void SetProcessHandle(base::ProcessHandle handle);

#if defined(OS_ANDROID)
  ServiceRegistryAndroid* service_registry_android() {
    return service_registry_android_.get();
  }
#endif

 private:
  class IOThreadContext;

  void GetInterface(const mojo::String& interface_name,
                    mojo::ScopedMessagePipeHandle request_handle);

  scoped_refptr<IOThreadContext> context_;

  const std::string service_token_;

  shell::InterfaceRegistry interface_registry_;
  shell::InterfaceProvider remote_interfaces_;

#if defined(OS_ANDROID)
  std::unique_ptr<ServiceRegistryAndroid> service_registry_android_;
#endif

  base::WeakPtrFactory<MojoChildConnection> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoChildConnection);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_CHILD_CONNECTION_H_
