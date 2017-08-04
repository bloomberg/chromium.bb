// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_test_base_aura.h"

#include "base/memory/ptr_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/shell/test/shell_test_extensions_browser_client.h"
#include "extensions/shell/test/shell_test_helper_aura.h"

namespace extensions {

ShellTestBaseAura::ShellTestBaseAura()
    : ExtensionsTest(base::MakeUnique<content::TestBrowserThreadBundle>()) {}

ShellTestBaseAura::~ShellTestBaseAura() {}

void ShellTestBaseAura::SetUp() {
  helper_ = base::MakeUnique<ShellTestHelperAura>();
  helper_->SetUp();
  std::unique_ptr<TestExtensionsBrowserClient> extensions_browser_client =
      base::MakeUnique<ShellTestExtensionsBrowserClient>();
  SetExtensionsBrowserClient(std::move(extensions_browser_client));
  ExtensionsTest::SetUp();
}

void ShellTestBaseAura::TearDown() {
  ExtensionsTest::TearDown();
  helper_->TearDown();
}

}  // namespace extensions
