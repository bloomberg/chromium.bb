// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_TEST_SHELL_TEST_H_
#define EXTENSIONS_SHELL_TEST_SHELL_TEST_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class ShellExtensionSystem;

// Base class for app shell browser tests.
class AppShellTest : public content::BrowserTestBase {
 public:
  AppShellTest();
  virtual ~AppShellTest();

  // content::BrowserTestBase implementation.
  virtual void SetUp() OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void RunTestOnMainThreadLoop() OVERRIDE;

  // Loads an unpacked application from a directory using |extension_system_|
  // and attempts to launch it. Returns true on success.
  bool LoadAndLaunchApp(const base::FilePath& app_dir);

  content::BrowserContext* browser_context() { return browser_context_; }

 private:
  content::BrowserContext* browser_context_;
  ShellExtensionSystem* extension_system_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_TEST_SHELL_TEST_H_
