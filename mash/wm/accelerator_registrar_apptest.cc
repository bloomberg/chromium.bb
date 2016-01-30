// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/mus/public/cpp/event_matcher.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/interfaces/accelerator_registrar.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_test_base.h"

using mus::mojom::AcceleratorHandler;
using mus::mojom::AcceleratorHandlerPtr;
using mus::mojom::AcceleratorRegistrar;
using mus::mojom::AcceleratorRegistrarPtr;

namespace mash {
namespace wm {

class TestAcceleratorHandler : public AcceleratorHandler {
 public:
  explicit TestAcceleratorHandler(AcceleratorRegistrarPtr registrar)
      : binding_(this),
        registrar_(std::move(registrar)),
        add_accelerator_result_(false) {
    registrar_->SetHandler(binding_.CreateInterfacePtrAndBind());
  }
  ~TestAcceleratorHandler() override {}

  // Attempts to install an accelerator with the specified id and event matcher.
  // Returns whether the accelerator could be successfully added or not.
  bool AttemptToInstallAccelerator(uint32_t accelerator_id,
                                   mus::mojom::EventMatcherPtr matcher) {
    DCHECK(!run_loop_);
    registrar_->AddAccelerator(
        accelerator_id, std::move(matcher),
        base::Bind(&TestAcceleratorHandler::AddAcceleratorCallback,
                   base::Unretained(this)));
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    run_loop_.reset();
    return add_accelerator_result_;
  }

 private:
  void AddAcceleratorCallback(bool success) {
    DCHECK(run_loop_ && run_loop_->running());
    add_accelerator_result_ = success;
    run_loop_->Quit();
  }

  // AcceleratorHandler:
  void OnAccelerator(uint32_t id, mus::mojom::EventPtr event) override {}

  std::set<uint32_t> installed_accelerators_;
  scoped_ptr<base::RunLoop> run_loop_;
  mojo::Binding<AcceleratorHandler> binding_;
  AcceleratorRegistrarPtr registrar_;
  bool add_accelerator_result_;

  DISALLOW_COPY_AND_ASSIGN(TestAcceleratorHandler);
};

class AcceleratorRegistrarTest : public mojo::test::ApplicationTestBase {
 public:
  AcceleratorRegistrarTest() {}
  ~AcceleratorRegistrarTest() override {}

 protected:
  void ConnectToRegistrar(AcceleratorRegistrarPtr* registrar) {
    application_impl()->ConnectToService("mojo:desktop_wm", registrar);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratorRegistrarTest);
};

TEST_F(AcceleratorRegistrarTest, AcceleratorRegistrarBasic) {
  AcceleratorRegistrarPtr registrar_first;
  ConnectToRegistrar(&registrar_first);
  TestAcceleratorHandler handler_first(std::move(registrar_first));
  EXPECT_TRUE(handler_first.AttemptToInstallAccelerator(
      1, mus::CreateKeyMatcher(mus::mojom::KeyboardCode::T,
                               mus::mojom::kEventFlagShiftDown)));
  // Attempting to add an accelerator with the same accelerator id from the same
  // registrar should fail.
  EXPECT_FALSE(handler_first.AttemptToInstallAccelerator(
      1, mus::CreateKeyMatcher(mus::mojom::KeyboardCode::N,
                               mus::mojom::kEventFlagShiftDown)));

  // Attempting to add an accelerator with the same id from a different
  // registrar should be OK.
  AcceleratorRegistrarPtr registrar_second;
  ConnectToRegistrar(&registrar_second);
  TestAcceleratorHandler handler_second(std::move(registrar_second));
  EXPECT_TRUE(handler_second.AttemptToInstallAccelerator(
      1, mus::CreateKeyMatcher(mus::mojom::KeyboardCode::N,
                               mus::mojom::kEventFlagShiftDown)));

  // But attempting to add an accelerator with the same matcher should fail.
  EXPECT_FALSE(handler_first.AttemptToInstallAccelerator(
      3, mus::CreateKeyMatcher(mus::mojom::KeyboardCode::N,
                               mus::mojom::kEventFlagShiftDown)));
  EXPECT_FALSE(handler_second.AttemptToInstallAccelerator(
      3, mus::CreateKeyMatcher(mus::mojom::KeyboardCode::N,
                               mus::mojom::kEventFlagShiftDown)));
}

}  // namespace wm
}  // namespace mash
