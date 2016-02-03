// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_STANDALONE_CONTEXT_H_
#define MOJO_SHELL_STANDALONE_CONTEXT_H_

#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/standalone/scoped_user_data_dir.h"
#include "mojo/shell/standalone/task_runners.h"
#include "mojo/shell/standalone/tracer.h"
#include "third_party/mojo/src/mojo/edk/embedder/process_delegate.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {
class NativeApplicationLoader;
class PackageManagerImpl;

// The "global" context for the shell's main process.
// TODO(use_chrome_edk)
// class Context : public edk::ProcessDelegate {
class Context : public embedder::ProcessDelegate {
 public:
  Context();
  ~Context() override;

  static void EnsureEmbedderIsInitialized();

  // This must be called with a message loop set up for the current thread,
  // which must remain alive until after Shutdown() is called.
  void Init(const base::FilePath& shell_file_root);

  // If Init() was called and succeeded, this must be called before destruction.
  void Shutdown();

  // NOTE: call either Run() or RunCommandLineApplication(), but not both.

  // Runs the app specified by |url|.
  void Run(const GURL& url);

  // Run the application specified on the commandline. When the app finishes,
  // |callback| is run if not null, otherwise the message loop is quit.
  void RunCommandLineApplication(const base::Closure& callback);

  TaskRunners* task_runners() { return task_runners_.get(); }
  ApplicationManager* application_manager() {
    return application_manager_.get();
  }

  PackageManagerImpl* package_manager() { return package_manager_; }

 private:
  class NativeViewportApplicationLoader;

  // ProcessDelegate implementation.
  void OnShutdownComplete() override;

  void OnApplicationEnd(const GURL& url);

  ScopedUserDataDir scoped_user_data_dir;
  std::set<GURL> app_urls_;
  scoped_ptr<TaskRunners> task_runners_;
  // Ensure this is destructed before task_runners_ since it owns a message pipe
  // that needs the IO thread to destruct cleanly.
  Tracer tracer_;
  // Owned by |application_manager_|.
  PackageManagerImpl* package_manager_;
  scoped_ptr<ApplicationManager> application_manager_;
  base::Closure app_complete_callback_;
  base::Time main_entry_time_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_STANDALONE_CONTEXT_H_
