// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_EMBEDDED_APPLICATION_RUNNER_H_
#define CONTENT_COMMON_MOJO_EMBEDDED_APPLICATION_RUNNER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace content {

// Hosts an in-process application instance that supports multiple ShellClient
// connections. The first incoming connection will invoke a provided factory
// function to instantiate the application, and the application will
// automatically be torn down when its last connection is lost. The application
// may be launched and torn down multiple times by a single
// EmbeddedApplicationRunner instance.
class EmbeddedApplicationRunner {
 public:
  // Callback used to construct a new instance of the embedded application. Note
  // that |quit_closure| destroys the returned ShellClient instance when run.
  using FactoryCallback = base::Callback<
      std::unique_ptr<shell::ShellClient>(const base::Closure& quit_closure)>;

  // Constructs a runner which hosts the application on |task_runner|'s thread.
  // If an existing instance of the app is not running when an incoming
  // connection is made, |callback| will be run on |task_runner|'s thread to
  // create a new instance which will live on that thread.
  //
  // If |task_runner| is null, the calling thread's TaskRunner is used.
  EmbeddedApplicationRunner(
      const FactoryCallback& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  ~EmbeddedApplicationRunner();

  // Binds an incoming ShellClientRequest for this application. If the
  // application isn't already running, it's started. Otherwise the request is
  // bound to the running instance.
  void BindShellClientRequest(shell::mojom::ShellClientRequest request);

  // Sets a callback to run after the application loses its last connection and
  // is torn down.
  void SetQuitClosure(const base::Closure& quit_closure);

 private:
  class Instance;

  void OnQuit();

  // The TaskRunner on which the factory callback will be run. The
  // shell::ShellClient it returns will live and die on this TaskRunner's
  // thread.
  const scoped_refptr<base::SingleThreadTaskRunner> application_task_runner_;

  // A reference to the application instance which may operate on the
  // |application_task_runner_|'s thread.
  scoped_refptr<Instance> instance_;

  base::Closure quit_closure_;

  base::WeakPtrFactory<EmbeddedApplicationRunner> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedApplicationRunner);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_EMBEDDED_APPLICATION_RUNNER_H_
