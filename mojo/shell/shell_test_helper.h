// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_TEST_HELPER_
#define MOJO_SHELL_SHELL_TEST_HELPER_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/shell/context.h"

class GURL;

namespace mojo {

class ServiceLoader;

namespace shell {

// ShellTestHelper is useful for tests to establish a connection to the
// ServiceManager. Invoke Init() to establish the connection. Once done,
// service_manager() returns the ServiceManager.
class ShellTestHelper {
 public:
  ShellTestHelper();
  ~ShellTestHelper();

  void Init();

  ServiceManager* service_manager() { return context_.service_manager(); }

  // Sets a ServiceLoader for the specified URL. |loader| is ultimately used on
  // the thread this class spawns.
  void SetLoaderForURL(scoped_ptr<ServiceLoader> loader, const GURL& url);

 private:
  Context context_;
  base::MessageLoop shell_loop_;
  scoped_ptr<ServiceManager::TestAPI> test_api_;
  DISALLOW_COPY_AND_ASSIGN(ShellTestHelper);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SHELL_TEST_HELPER_
