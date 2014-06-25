// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_TEST_HELPER_
#define MOJO_SHELL_SHELL_TEST_HELPER_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/shell/context.h"

class GURL;

namespace mojo {

class ServiceLoader;

namespace shell {

// ShellTestHelper is useful for tests to establish a connection to the
// ServiceProvider. Invoke Init() to establish the connection. Once done,
// service_provider() returns the handle to the ServiceProvider.
class ShellTestHelper {
 public:
  ShellTestHelper();
  ~ShellTestHelper();

  void Init();

  // Returns a handle to the ServiceManager. ShellTestHelper owns the
  // ServiceProvider.
  ServiceManager* service_manager() { return context_->service_manager(); }

  // Sets a ServiceLoader for the specified URL. |loader| is ultimately used on
  // the thread this class spawns.
  void SetLoaderForURL(scoped_ptr<ServiceLoader> loader, const GURL& url);

 private:
  scoped_ptr<Context> context_;
  scoped_ptr<ServiceManager::TestAPI> test_api_;
  DISALLOW_COPY_AND_ASSIGN(ShellTestHelper);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SHELL_TEST_HELPER_
