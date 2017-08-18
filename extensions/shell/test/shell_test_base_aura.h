// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_TEST_SHELL_TEST_BASE_AURA_H_
#define EXTENSIONS_SHELL_TEST_SHELL_TEST_BASE_AURA_H_

#include <memory>

#include "base/macros.h"

#include "extensions/browser/extensions_test.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {
class AppWindow;
class ShellTestHelperAura;

class ShellTestBaseAura : public ExtensionsTest {
 public:
  ShellTestBaseAura();
  ~ShellTestBaseAura() override;

  // ExtensionsTest:
  void SetUp() override;
  void TearDown() override;

  // Initializes |app_window| for testing.
  void InitAppWindow(AppWindow* app_window, const gfx::Rect& bounds = {});

 private:
  std::unique_ptr<ShellTestHelperAura> helper_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestBaseAura);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_TEST_SHELL_TEST_BASE_AURA_H_
