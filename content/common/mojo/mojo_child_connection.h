// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_MOJO_CHILD_CONNECTION_H_
#define CONTENT_COMMON_MOJO_MOJO_CHILD_CONNECTION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/interfaces/connector.mojom.h"

namespace shell {
class Connection;
class Connector;
}

namespace content {

// Helper class to establish a connection between the shell and a single child
// process. Process hosts can use this when launching new processes which
// should be registered with the shell.
class CONTENT_EXPORT MojoChildConnection {
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

  shell::InterfaceProvider* GetRemoteInterfaces() {
    return &remote_interfaces_;
  }

  const shell::Identity& child_identity() const {
    return child_identity_;
  }

  // A token which must be passed to the child process via
  // |switches::kPrimordialPipeToken| in order for the child to initialize its
  // end of the shell connection pipe.
  std::string service_token() const { return service_token_; }

  // Sets the child connection's process handle. This should be called as soon
  // as the process has been launched, and the connection will not be fully
  // functional until this is called.
  void SetProcessHandle(base::ProcessHandle handle);

 private:
  class IOThreadContext;

  scoped_refptr<IOThreadContext> context_;
  shell::Identity child_identity_;
  const std::string service_token_;

  shell::InterfaceProvider remote_interfaces_;

  base::WeakPtrFactory<MojoChildConnection> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoChildConnection);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_MOJO_CHILD_CONNECTION_H_
