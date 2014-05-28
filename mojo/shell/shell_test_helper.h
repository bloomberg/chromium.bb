// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_TEST_HELPER_
#define MOJO_SHELL_SHELL_TEST_HELPER_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"

class GURL;

namespace base {
class MessageLoopProxy;
class RunLoop;
}

namespace mojo {

class ServiceLoader;

namespace shell {

// ShellTestHelper is useful for tests to establish a connection to the
// ServiceProvider. ShellTestHelper does this by spawning a thread and
// connecting. Invoke Init() to do this. Once done, service_provider()
// returns the handle to the ServiceProvider.
class ShellTestHelper {
 public:
  struct State;

  ShellTestHelper();
  ~ShellTestHelper();

  void Init();

  // Returns a handle to the ServiceProvider. ShellTestHelper owns the
  // ServiceProvider.
  ServiceProvider* service_provider() { return service_provider_.get(); }

  // Sets a ServiceLoader for the specified URL. |loader| is ultimately used on
  // the thread this class spawns.
  void SetLoaderForURL(scoped_ptr<ServiceLoader> loader, const GURL& url);

 private:
  class TestServiceProvider;

  // Invoked once connection has been established.
  void OnServiceProviderStarted();

  Environment environment_;

  base::Thread service_provider_thread_;

  // If non-null we're in Init() and waiting for connection.
  scoped_ptr<base::RunLoop> run_loop_;

  // See comment in declaration for details.
  State* state_;

  // Client interface for the shell.
  scoped_ptr<TestServiceProvider> local_service_provider_;

  ServiceProviderPtr service_provider_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestHelper);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SHELL_TEST_HELPER_
