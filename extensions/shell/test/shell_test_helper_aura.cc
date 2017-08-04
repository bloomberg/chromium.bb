// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_test_helper_aura.h"

#include "ui/aura/test/aura_test_helper.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/context_factories_for_test.h"

namespace extensions {

ShellTestHelperAura::ShellTestHelperAura() {}

ShellTestHelperAura::~ShellTestHelperAura() {}

void ShellTestHelperAura::SetUp() {
  // The ContextFactory must exist before any Compositors are created.
  ui::ContextFactory* context_factory = nullptr;
  ui::ContextFactoryPrivate* context_factory_private = nullptr;
  ui::InitializeContextFactoryForTests(/*enable_pixel_output=*/false,
                                       &context_factory,
                                       &context_factory_private);

  // AuraTestHelper sets up the rest of the Aura initialization.
  helper_.reset(new aura::test::AuraTestHelper());
  helper_->SetUp(context_factory, context_factory_private);
}

void ShellTestHelperAura::TearDown() {
  helper_->RunAllPendingInMessageLoop();
  helper_->TearDown();
  ui::TerminateContextFactoryForTests();
}

}  // namespace extensions
