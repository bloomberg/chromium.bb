// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_CHILD_PROCESS_HOST_H_
#define CHROME_SERVICE_SERVICE_CHILD_PROCESS_HOST_H_
#pragma once

#include "base/process.h"
#include "content/common/child_process_host.h"

class CommandLine;

// Plugins/workers and other child processes that live on the IO thread should
// derive from this class.
//
class ServiceChildProcessHost : public ChildProcessHost {
 public:
  virtual ~ServiceChildProcessHost();

 protected:
  ServiceChildProcessHost();

  // Derived classes call this to launch the child process synchronously.
  // TODO(sanjeevr): Determine whether we need to make the launch asynchronous.
  // |exposed_dir| is the path to tbe exposed to the sandbox. This is ignored
  // if |no_sandbox| is true.
  bool Launch(CommandLine* cmd_line,
              bool no_sandbox,
              const FilePath& exposed_dir);

  base::ProcessHandle handle() const { return handle_; }

 private:
  base::ProcessHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(ServiceChildProcessHost);
};

#endif  // CHROME_SERVICE_SERVICE_CHILD_PROCESS_HOST_H_
