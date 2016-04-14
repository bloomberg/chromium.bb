// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_CHILD_CONNECTION_H_
#define CONTENT_BROWSER_MOJO_MOJO_CHILD_CONNECTION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "services/shell/public/interfaces/connector.mojom.h"

namespace shell {
class Connection;
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
  MojoChildConnection(const std::string& application_name,
                      const std::string& instance_id);
  ~MojoChildConnection();

  shell::Connection* connection() const {
    return connection_.get();
  }

  // A token which must be passed to the child process via
  // |switches::kPrimordialPipeToken| in order for the child to initialize its
  // end of the shell connection pipe.
  std::string shell_client_token() const { return shell_client_token_; }

  // Sets the child connection's process handle. This should be called as soon
  // as the process has been launched, and the connection will not be fully
  // functional until this is called.
  void SetProcessHandle(base::ProcessHandle handle);

 private:
  const std::string shell_client_token_;
  std::unique_ptr<shell::Connection> connection_;
  shell::mojom::PIDReceiverPtr pid_receiver_;

  DISALLOW_COPY_AND_ASSIGN(MojoChildConnection);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_CHILD_CONNECTION_H_
