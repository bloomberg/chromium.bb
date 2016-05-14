// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "components/mus/public/interfaces/window_server_test.mojom.h"
#include "mash/shelf/public/interfaces/shelf.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/shell/public/cpp/shell_test.h"

namespace {

class TestShelfObserver : public mash::shelf::mojom::ShelfObserver {
 public:
  explicit TestShelfObserver(mash::shelf::mojom::ShelfControllerPtr* shelf)
      : observer_binding_(this) {
    mash::shelf::mojom::ShelfObserverAssociatedPtrInfo ptr_info;
    observer_binding_.Bind(&ptr_info, shelf->associated_group());
    (*shelf)->AddObserver(std::move(ptr_info));
  }

  ~TestShelfObserver() override {}

  mash::shelf::mojom::Alignment alignment() const { return alignment_; }
  mash::shelf::mojom::AutoHideBehavior auto_hide() const { return auto_hide_; }

  void WaitForIncomingMethodCalls(size_t expected_calls) {
    DCHECK_EQ(0u, expected_calls_);
    expected_calls_ = expected_calls;
    DCHECK(!run_loop_ || !run_loop_->running());
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

 private:
  void OnMethodCall() {
    DCHECK_LT(0u, expected_calls_);
    DCHECK(run_loop_->running());
    expected_calls_--;
    if (expected_calls_ == 0u)
      run_loop_->Quit();
  }

  // mash::shelf::mojom::ShelfObserver:
  void OnAlignmentChanged(mash::shelf::mojom::Alignment alignment) override {
    alignment_ = alignment;
    OnMethodCall();
  }
  void OnAutoHideBehaviorChanged(
      mash::shelf::mojom::AutoHideBehavior auto_hide) override {
    auto_hide_ = auto_hide;
    OnMethodCall();
  }

  mojo::AssociatedBinding<mash::shelf::mojom::ShelfObserver> observer_binding_;

  mash::shelf::mojom::Alignment alignment_ =
      mash::shelf::mojom::Alignment::BOTTOM;
  mash::shelf::mojom::AutoHideBehavior auto_hide_ =
      mash::shelf::mojom::AutoHideBehavior::NEVER;

  size_t expected_calls_ = 0u;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfObserver);
};

}  // namespace

namespace ash {
namespace sysui {

void RunCallback(bool* success, const base::Closure& callback, bool result) {
  *success = result;
  callback.Run();
}

class ShelfDelegateMusTest : public shell::test::ShellTest {
 public:
  ShelfDelegateMusTest() : ShellTest("exe:mash_unittests") {}
  ~ShelfDelegateMusTest() override {}

 private:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch("use-test-config");
    ShellTest::SetUp();
  }

  DISALLOW_COPY_AND_ASSIGN(ShelfDelegateMusTest);
};

TEST_F(ShelfDelegateMusTest, AshSysUIHasDrawnWindow) {
  // mash_session embeds ash_sysui, which paints the shelf.
  connector()->Connect("mojo:mash_session");
  mus::mojom::WindowServerTestPtr test_interface;
  connector()->ConnectToInterface("mojo:mus", &test_interface);
  base::RunLoop run_loop;
  bool drawn = false;
  test_interface->EnsureClientHasDrawnWindow(
      "mojo:ash_sysui",
      base::Bind(&RunCallback, &drawn, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(drawn);
}

TEST_F(ShelfDelegateMusTest, ShelfControllerAndObserverBasic) {
  connector()->Connect("mojo:mash_session");
  mash::shelf::mojom::ShelfControllerPtr shelf_controller;
  connector()->ConnectToInterface("mojo:ash_sysui", &shelf_controller);

  // Adding the observer should fire state initialization function calls.
  TestShelfObserver observer(&shelf_controller);
  observer.WaitForIncomingMethodCalls(2u);
  EXPECT_EQ(mash::shelf::mojom::Alignment::BOTTOM, observer.alignment());
  EXPECT_EQ(mash::shelf::mojom::AutoHideBehavior::NEVER, observer.auto_hide());

  shelf_controller->SetAlignment(mash::shelf::mojom::Alignment::LEFT);
  observer.WaitForIncomingMethodCalls(1u);
  EXPECT_EQ(mash::shelf::mojom::Alignment::LEFT, observer.alignment());

  shelf_controller->SetAutoHideBehavior(
      mash::shelf::mojom::AutoHideBehavior::ALWAYS);
  observer.WaitForIncomingMethodCalls(1u);
  EXPECT_EQ(mash::shelf::mojom::AutoHideBehavior::ALWAYS, observer.auto_hide());
}

}  // namespace sysui
}  // namespace ash
