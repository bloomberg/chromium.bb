// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_NATIVE_RUNNER_H_
#define MOJO_SHELL_NATIVE_RUNNER_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process_handle.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"
#include "mojo/shell/public/interfaces/shell_client_factory.mojom.h"

namespace base {
class FilePath;
}

namespace mojo {
namespace shell {

class Identity;

// Shell requires implementations of NativeRunner and NativeRunnerFactory to run
// native applications.
class NativeRunner {
 public:
  virtual ~NativeRunner() {}

  // Loads the app in the file at |app_path| and runs it on some other
  // thread/process.
  virtual void Start(
      const base::FilePath& app_path,
      const Identity& target,
      bool start_sandboxed,
      InterfaceRequest<mojom::ShellClient> request,
      const base::Callback<void(base::ProcessId)>& pid_available_callback,
      const base::Closure& app_completed_callback) = 0;
};

class NativeRunnerFactory {
 public:
  virtual ~NativeRunnerFactory() {}
  virtual scoped_ptr<NativeRunner> Create(const base::FilePath& app_path) = 0;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_NATIVE_RUNNER_H_
