// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/athena_test_base.h"

#include "athena/test/athena_test_helper.h"
#include "ui/compositor/test/context_factories_for_test.h"

#if defined(USE_X11)
#include "ui/aura/window_tree_host_x11.h"
#endif

namespace athena {
namespace test {

AthenaTestBase::AthenaTestBase()
    : setup_called_(false), teardown_called_(false) {
}

AthenaTestBase::~AthenaTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called super class's SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called super class's TearDown";
}

void AthenaTestBase::SetUp() {
  setup_called_ = true;
  testing::Test::SetUp();

  // The ContextFactory must exist before any Compositors are created.
  bool enable_pixel_output = false;
  ui::ContextFactory* context_factory =
      ui::InitializeContextFactoryForTests(enable_pixel_output);

  helper_.reset(new AthenaTestHelper(&message_loop_));
#if defined(USE_X11)
  aura::test::SetUseOverrideRedirectWindowByDefault(true);
#endif
  helper_->SetUp(context_factory);
}

void AthenaTestBase::TearDown() {
  teardown_called_ = true;

  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunAllPendingInMessageLoop();

  helper_->TearDown();
  ui::TerminateContextFactoryForTests();
  testing::Test::TearDown();
}

void AthenaTestBase::RunAllPendingInMessageLoop() {
  helper_->RunAllPendingInMessageLoop();
}

}  // namespace test
}  // namespace athena
