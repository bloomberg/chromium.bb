// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_OUT_OF_PROCESS_NATIVE_RUNNER_H_
#define MOJO_RUNNER_OUT_OF_PROCESS_NATIVE_RUNNER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/shell/native_runner.h"

namespace mojo {
namespace runner {

class ChildProcessHost;
class Context;

// An implementation of |NativeRunner| that loads/runs the given app (from the
// file system) in a separate process (of its own).
class OutOfProcessNativeRunner : public shell::NativeRunner {
 public:
  explicit OutOfProcessNativeRunner(Context* context);
  ~OutOfProcessNativeRunner() override;

  // |NativeRunner| method:
  void Start(const base::FilePath& app_path,
             shell::NativeApplicationCleanup cleanup,
             InterfaceRequest<Application> application_request,
             const base::Closure& app_completed_callback) override;

 private:
  // |ChildController::StartApp()| callback:
  void AppCompleted(int32_t result);

  Context* const context_;

  base::FilePath app_path_;
  base::Closure app_completed_callback_;

  scoped_ptr<ChildProcessHost> child_process_host_;

  DISALLOW_COPY_AND_ASSIGN(OutOfProcessNativeRunner);
};

class OutOfProcessNativeRunnerFactory : public shell::NativeRunnerFactory {
 public:
  explicit OutOfProcessNativeRunnerFactory(Context* context)
      : context_(context) {}
  ~OutOfProcessNativeRunnerFactory() override {}

  scoped_ptr<shell::NativeRunner> Create(const Options& options) override;

 private:
  Context* const context_;

  DISALLOW_COPY_AND_ASSIGN(OutOfProcessNativeRunnerFactory);
};

}  // namespace runner
}  // namespace mojo

#endif  // MOJO_RUNNER_OUT_OF_PROCESS_NATIVE_RUNNER_H_
