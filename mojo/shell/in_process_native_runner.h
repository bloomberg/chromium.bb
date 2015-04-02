// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_IN_PROCESS_NATIVE_RUNNER_H_
#define SHELL_IN_PROCESS_NATIVE_RUNNER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_native_library.h"
#include "base/threading/simple_thread.h"
#include "mojo/shell/application_manager/native_runner.h"
#include "mojo/shell/native_application_support.h"

namespace mojo {
namespace shell {

class Context;

// An implementation of |NativeRunner| that loads/runs the given app (from the
// file system) on a separate thread (in the current process).
class InProcessNativeRunner : public NativeRunner,
                              public base::DelegateSimpleThread::Delegate {
 public:
  explicit InProcessNativeRunner(Context* context);
  ~InProcessNativeRunner() override;

  // |NativeRunner| method:
  void Start(const base::FilePath& app_path,
             NativeApplicationCleanup cleanup,
             InterfaceRequest<Application> application_request,
             const base::Closure& app_completed_callback) override;

 private:
  // |base::DelegateSimpleThread::Delegate| method:
  void Run() override;

  base::FilePath app_path_;
  NativeApplicationCleanup cleanup_;
  InterfaceRequest<Application> application_request_;
  base::Callback<bool(void)> app_completed_callback_runner_;

  base::ScopedNativeLibrary app_library_;
  scoped_ptr<base::DelegateSimpleThread> thread_;

  DISALLOW_COPY_AND_ASSIGN(InProcessNativeRunner);
};

class InProcessNativeRunnerFactory : public NativeRunnerFactory {
 public:
  explicit InProcessNativeRunnerFactory(Context* context) : context_(context) {}
  ~InProcessNativeRunnerFactory() override {}

  scoped_ptr<NativeRunner> Create(const Options& options) override;

 private:
  Context* const context_;

  DISALLOW_COPY_AND_ASSIGN(InProcessNativeRunnerFactory);
};

}  // namespace shell
}  // namespace mojo

#endif  // SHELL_IN_PROCESS_NATIVE_RUNNER_H_
