// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/test_event_injector.mojom.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ash {
namespace {

// TestWindowDelegate that remembers the last MouseEvent.
class TestWindowDelegateImpl : public aura::test::TestWindowDelegate {
 public:
  TestWindowDelegateImpl() = default;
  ~TestWindowDelegateImpl() override = default;

  ui::MouseEvent* last_mouse_event() { return last_mouse_event_.get(); }

  // aura::test::TestWindowDelegate:
  void OnMouseEvent(ui::MouseEvent* event) override {
    last_mouse_event_ = std::make_unique<ui::MouseEvent>(*event);
  }

 private:
  std::unique_ptr<ui::MouseEvent> last_mouse_event_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegateImpl);
};

}  // namespace

using MojoTestInterfaceFactoryTest = AshTestBase;

TEST_F(MojoTestInterfaceFactoryTest, InjectEvent) {
  // Create a window that the event should go to.
  TestWindowDelegateImpl test_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &test_delegate, 100, gfx::Rect(100, 100, 200, 200)));

  // Ask for the |test_event_injector|.
  ui::mojom::TestEventInjectorPtr test_event_injector;
  ash_test_helper()->GetWindowServiceConnector()->BindInterface(
      ui::mojom::kServiceName, &test_event_injector);
  bool was_callback_run = false;
  bool run_result = false;
  base::RunLoop run_loop;
  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(120, 130),
                             gfx::Point(120, 130), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  // The callback run once the event has been completely processed.
  auto callback = base::BindOnce(
      [](base::RunLoop* run_loop, bool* was_run, bool* result_holder,
         bool result) {
        run_loop->Quit();
        *was_run = true;
        *result_holder = result;
      },
      &run_loop, &was_callback_run, &run_result);

  // Inject the event. Processing is async, so wait for the callback to be run.
  // TODO: removing mapping to PointerEvent: https://crbug.com/865781
  test_event_injector->InjectEvent(
      GetPrimaryDisplay().id(), std::make_unique<ui::PointerEvent>(mouse_event),
      std::move(callback));
  run_loop.Run();

  // Ensure we got the event.
  EXPECT_TRUE(was_callback_run);
  ASSERT_TRUE(test_delegate.last_mouse_event());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, test_delegate.last_mouse_event()->type());
  EXPECT_EQ(gfx::Point(20, 30), test_delegate.last_mouse_event()->location());
}

}  // namespace ash
