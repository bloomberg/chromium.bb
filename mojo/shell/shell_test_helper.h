// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_TEST_HELPER_
#define MOJO_SHELL_SHELL_TEST_HELPER_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "mojo/application_manager/application_loader.h"
#include "mojo/shell/context.h"

class GURL;

namespace mojo {

class ApplicationLoader;

namespace shell {

// ShellTestHelper is useful for tests to establish a connection to the
// ApplicationManager. Invoke Init() to establish the connection. Once done,
// application_manager() returns the ApplicationManager.
class ShellTestHelper {
 public:
  ShellTestHelper();
  ~ShellTestHelper();

  void Init();

  ApplicationManager* application_manager() {
    return context_.application_manager();
  }

  // Sets a ApplicationLoader for the specified URL. |loader| is ultimately used
  // on
  // the thread this class spawns.
  void SetLoaderForURL(scoped_ptr<ApplicationLoader> loader, const GURL& url);

 private:
  Context context_;
  base::MessageLoop shell_loop_;
  scoped_ptr<ApplicationManager::TestAPI> test_api_;
  DISALLOW_COPY_AND_ASSIGN(ShellTestHelper);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SHELL_TEST_HELPER_
