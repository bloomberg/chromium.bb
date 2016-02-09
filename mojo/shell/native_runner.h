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

#if defined(OS_WIN)
#undef DELETE
#endif

namespace base {
class FilePath;
}

namespace mojo {
namespace shell {

// ApplicationManager requires implementations of NativeRunner and
// NativeRunnerFactory to run native applications.
class NativeRunner {
 public:
  virtual ~NativeRunner() {}

  // Loads the app in the file at |app_path| and runs it on some other
  // thread/process.
  virtual void Start(
      const base::FilePath& app_path,
      bool start_sandboxed,
      InterfaceRequest<mojom::ShellClient> request,
      const base::Callback<void(base::ProcessId)>& pid_available_callback,
      const base::Closure& app_completed_callback) = 0;

  // Like Start(), but used to initialize the host for a child process started
  // by someone else. Provides |request| via |channel|.
  virtual void InitHost(ScopedHandle channel,
                        InterfaceRequest<mojom::ShellClient> request) = 0;
};

class NativeRunnerFactory {
 public:
  virtual ~NativeRunnerFactory() {}
  virtual scoped_ptr<NativeRunner> Create(const base::FilePath& app_path) = 0;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_NATIVE_RUNNER_H_
