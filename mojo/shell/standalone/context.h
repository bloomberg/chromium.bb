// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_STANDALONE_CONTEXT_H_
#define MOJO_SHELL_STANDALONE_CONTEXT_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/standalone/tracer.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace package_manager {
class ApplicationCatalogStore;
}

namespace mojo {
namespace shell {
class NativeRunnerDelegate;

// The "global" context for the shell's main process.
class Context : public edk::ProcessDelegate {
 public:
  struct InitParams {
    InitParams();
    ~InitParams();

    NativeRunnerDelegate* native_runner_delegate = nullptr;
    scoped_ptr<package_manager::ApplicationCatalogStore> app_catalog;
  };

  Context();
  ~Context() override;

  static void EnsureEmbedderIsInitialized();

  // This must be called with a message loop set up for the current thread,
  // which must remain alive until after Shutdown() is called.
  void Init(scoped_ptr<InitParams> init_params);

  // If Init() was called and succeeded, this must be called before destruction.
  void Shutdown();

  // Run the application specified on the command line.
  void RunCommandLineApplication();

  ApplicationManager* application_manager() {
    return application_manager_.get();
  }

 private:
  // edk::ProcessDelegate:
  void OnShutdownComplete() override;

  // Runs the app specified by |name|.
  void Run(const std::string& name);

  scoped_refptr<base::SingleThreadTaskRunner> shell_runner_;
  scoped_ptr<base::Thread> io_thread_;
  scoped_refptr<base::SequencedWorkerPool> blocking_pool_;

  // Ensure this is destructed before task_runners_ since it owns a message pipe
  // that needs the IO thread to destruct cleanly.
  Tracer tracer_;
  scoped_ptr<ApplicationManager> application_manager_;
  base::Time main_entry_time_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_STANDALONE_CONTEXT_H_
