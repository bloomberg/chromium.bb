// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/athena_test_helper.h"

#include "athena/env/public/athena_env.h"
#include "athena/extensions/public/extensions_delegate.h"
#include "athena/main/public/athena_launcher.h"
#include "athena/test/sample_activity_factory.h"
#include "athena/test/test_app_model_builder.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/input_method_event_filter.h"

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

namespace athena {
namespace test {

AthenaTestHelper::AthenaTestHelper(base::MessageLoopForUI* message_loop)
    : setup_called_(false), teardown_called_(false) {
  DCHECK(message_loop);
  message_loop_ = message_loop;
  // Disable animations during tests.
  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
}

AthenaTestHelper::~AthenaTestHelper() {
  CHECK(setup_called_) << "AthenaTestHelper::SetUp() never called.";
  CHECK(teardown_called_) << "AthenaTestHelper::TearDown() never called.";
}

void AthenaTestHelper::SetUp(ui::ContextFactory* context_factory) {
  setup_called_ = true;
  file_thread_.reset(new base::Thread("FileThread"));
  base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
  file_thread_->StartWithOptions(options);

  chromeos::DBusThreadManager::Initialize();
  chromeos::NetworkHandler::Initialize();
  ui::InitializeInputMethodForTesting();
  aura::Env::CreateInstance(true);
  aura::Env::GetInstance()->set_context_factory(context_factory);

  // Unit tests generally don't want to query the system, rather use the state
  // from RootWindow.
  aura::test::EnvTestHelper(aura::Env::GetInstance())
      .SetInputStateLookup(scoped_ptr<aura::InputStateLookup>());

  // TODO(oshima): Use a BlockingPool task runner.
  athena::StartAthenaEnv(file_thread_->message_loop_proxy());
  athena::ExtensionsDelegate::CreateExtensionsDelegateForTest();
  athena::StartAthenaSession(new SampleActivityFactory(),
                             new TestAppModelBuilder());
}

void AthenaTestHelper::TearDown() {
  teardown_called_ = true;

  athena::ShutdownAthena();
  aura::Env::DeleteInstance();

#if defined(USE_X11)
  ui::test::ResetXCursorCache();
#endif

  ui::ShutdownInputMethodForTesting();
  chromeos::NetworkHandler::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
}

aura::Window* AthenaTestHelper::GetRootWindow() {
  return GetHost()->window();
}

aura::WindowTreeHost* AthenaTestHelper::GetHost() {
  return AthenaEnv::Get()->GetHost();
}

void AthenaTestHelper::RunAllPendingInMessageLoop() {
  // TODO(jbates) crbug.com/134753 Find quitters of this RunLoop and have them
  //              use run_loop.QuitClosure().
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

}  // namespace test
}  // namespace athena
