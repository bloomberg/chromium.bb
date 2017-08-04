// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_TEST_SHELL_TEST_HELPER_AURA_H_
#define EXTENSIONS_SHELL_BROWSER_TEST_SHELL_TEST_HELPER_AURA_H_

#include <memory>

#include "base/macros.h"

namespace aura {
namespace test {
class AuraTestHelper;
}
}  // namespace aura

namespace extensions {

// A helper class that does common Aura initialization required for the shell.
class ShellTestHelperAura {
 public:
  ShellTestHelperAura();
  ~ShellTestHelperAura();

  // Initializes common test dependencies.
  void SetUp();

  // Cleans up.
  void TearDown();

 private:
  std::unique_ptr<aura::test::AuraTestHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestHelperAura);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_TEST_SHELL_TEST_HELPER_AURA_H_
