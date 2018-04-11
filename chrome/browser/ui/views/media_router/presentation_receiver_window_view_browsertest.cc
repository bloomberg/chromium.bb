// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/presentation_receiver_window_view.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/media_router/presentation_receiver_window_delegate.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/media_router/presentation_receiver_window_frame.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "url/gurl.h"

#if defined(CHROMEOS)
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#endif

namespace {

using content::WebContents;

// Provides a WebContents for the PresentationReceiverWindowView to display and
// a window-close callback for the test to cleanly close the view.
class FakeReceiverDelegate final : public PresentationReceiverWindowDelegate {
 public:
  explicit FakeReceiverDelegate(Profile* profile)
      : web_contents_(WebContents::Create(WebContents::CreateParams(profile))) {
  }

  void set_window_closed_callback(base::OnceClosure callback) {
    closed_callback_ = std::move(callback);
  }

  // PresentationReceiverWindowDelegate overrides.
  void WindowClosed() final {
    if (closed_callback_)
      std::move(closed_callback_).Run();
  }
  content::WebContents* web_contents() const final {
    return web_contents_.get();
  }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  base::OnceClosure closed_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeReceiverDelegate);
};

class PresentationReceiverWindowViewBrowserTest : public InProcessBrowserTest {
 protected:
  PresentationReceiverWindowViewBrowserTest() = default;

  PresentationReceiverWindowView* CreateReceiverWindowView(
      PresentationReceiverWindowDelegate* delegate,
      const gfx::Rect& bounds) {
    auto* frame =
        new PresentationReceiverWindowFrame(Profile::FromBrowserContext(
            delegate->web_contents()->GetBrowserContext()));
    auto view =
        std::make_unique<PresentationReceiverWindowView>(frame, delegate);
    auto* view_raw = view.get();
    frame->InitReceiverFrame(std::move(view), bounds);
    view_raw->Init();
    return view_raw;
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    fake_delegate_ =
        std::make_unique<FakeReceiverDelegate>(browser()->profile());
    receiver_view_ = CreateReceiverWindowView(fake_delegate_.get(), bounds_);
  }

  void TearDownOnMainThread() override {
    base::RunLoop run_loop;
    fake_delegate_->set_window_closed_callback(run_loop.QuitClosure());
    static_cast<PresentationReceiverWindow*>(receiver_view_)->Close();
    run_loop.Run();
    fake_delegate_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  const gfx::Rect bounds_{100, 100};
  std::unique_ptr<FakeReceiverDelegate> fake_delegate_ = nullptr;
  PresentationReceiverWindowView* receiver_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PresentationReceiverWindowViewBrowserTest);
};

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PresentationReceiverWindowViewBrowserTest,
                       ChromeOSHardwareFullscreenButton) {
  static_cast<PresentationReceiverWindow*>(receiver_view_)
      ->ShowInactiveFullscreen();
  ASSERT_TRUE(
      static_cast<ExclusiveAccessContext*>(receiver_view_)->IsFullscreen());
  EXPECT_FALSE(receiver_view_->location_bar_view()->visible());
  // Bypass ExclusiveAccessContext and default accelerator to simulate hardware
  // window state button, which sets the native aura window to a "normal" state.

  // This class checks the receiver view's fullscreen state when it's native
  // aura::Window's bounds change.  It calls |fullscreen_callback| when the
  // desired fullscreen state is seen, set via |await_type|.  It automatically
  // registers and unregisters itself as an observer of the receiver view's
  // aura::Window.  This is necessary because Mus only schedules the fullscreen
  // change during SetProperty  and because the fullscreen change takes place in
  // another process, we can't simply call base::RunLoop().RunUntilIdle().
  class FullscreenWaiter final : public aura::WindowObserver {
   public:
    enum class AwaitType {
      kOutOfFullscreen,
      kIntoFullscreen,
    };

    FullscreenWaiter(PresentationReceiverWindowView* receiver_view,
                     AwaitType await_type,
                     base::OnceClosure fullscreen_callback)
        : receiver_view_(receiver_view),
          await_type_(await_type),
          fullscreen_callback_(std::move(fullscreen_callback)) {
      static_cast<views::View*>(receiver_view_)
          ->GetWidget()
          ->GetNativeWindow()
          ->AddObserver(this);
    }
    ~FullscreenWaiter() final {
      static_cast<views::View*>(receiver_view_)
          ->GetWidget()
          ->GetNativeWindow()
          ->RemoveObserver(this);
    }

   private:
    // aura::WindowObserver overrides.
    void OnWindowBoundsChanged(aura::Window* window,
                               const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds,
                               ui::PropertyChangeReason reason) final {
      DCHECK(fullscreen_callback_);
      if (static_cast<ExclusiveAccessContext*>(receiver_view_)
              ->IsFullscreen() == (await_type_ == AwaitType::kIntoFullscreen)) {
        std::move(fullscreen_callback_).Run();
      }
    }

    PresentationReceiverWindowView* const receiver_view_;
    const AwaitType await_type_;
    base::OnceClosure fullscreen_callback_;

    DISALLOW_COPY_AND_ASSIGN(FullscreenWaiter);
  };
  auto* native_window =
      static_cast<views::View*>(receiver_view_)->GetWidget()->GetNativeWindow();
  {
    base::RunLoop fullscreen_loop;
    FullscreenWaiter waiter(receiver_view_,
                            FullscreenWaiter::AwaitType::kOutOfFullscreen,
                            fullscreen_loop.QuitClosure());
    native_window->SetProperty(aura::client::kShowStateKey,
                               ui::SHOW_STATE_NORMAL);
    fullscreen_loop.Run();
    ASSERT_FALSE(
        static_cast<ExclusiveAccessContext*>(receiver_view_)->IsFullscreen());
    EXPECT_TRUE(receiver_view_->location_bar_view()->visible());
  }

  // Back to fullscreen with the hardware button.
  {
    base::RunLoop fullscreen_loop;
    FullscreenWaiter waiter(receiver_view_,
                            FullscreenWaiter::AwaitType::kIntoFullscreen,
                            fullscreen_loop.QuitClosure());
    native_window->SetProperty(aura::client::kShowStateKey,
                               ui::SHOW_STATE_FULLSCREEN);
    fullscreen_loop.Run();
    ASSERT_TRUE(
        static_cast<ExclusiveAccessContext*>(receiver_view_)->IsFullscreen());
    EXPECT_FALSE(receiver_view_->location_bar_view()->visible());
  }
  static_cast<views::View*>(receiver_view_)
      ->GetWidget()
      ->GetNativeWindow()
      ->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  ASSERT_FALSE(
      static_cast<ExclusiveAccessContext*>(receiver_view_)->IsFullscreen());
  EXPECT_TRUE(receiver_view_->location_bar_view()->visible());

  // Back to fullscreen with the hardware button.
  static_cast<views::View*>(receiver_view_)
      ->GetWidget()
      ->GetNativeWindow()
      ->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  ASSERT_TRUE(
      static_cast<ExclusiveAccessContext*>(receiver_view_)->IsFullscreen());
  EXPECT_FALSE(receiver_view_->location_bar_view()->visible());
}
#endif

#if defined(OS_MACOSX)
// https://crbug.com/828031
#define MAYBE_LocationBarViewShown DISABLED_LocationBarViewShown
#else
#define MAYBE_LocationBarViewShown LocationBarViewShown
#endif

IN_PROC_BROWSER_TEST_F(PresentationReceiverWindowViewBrowserTest,
                       MAYBE_LocationBarViewShown) {
  static_cast<PresentationReceiverWindow*>(receiver_view_)
      ->ShowInactiveFullscreen();
  static_cast<ExclusiveAccessContext*>(receiver_view_)->ExitFullscreen();
  ASSERT_FALSE(
      static_cast<ExclusiveAccessContext*>(receiver_view_)->IsFullscreen());

  auto* location_bar_view = receiver_view_->location_bar_view();
  EXPECT_TRUE(location_bar_view->IsDrawn());
  EXPECT_LE(0, location_bar_view->x());
  EXPECT_LE(0, location_bar_view->y());
  EXPECT_LT(0, location_bar_view->width());
  EXPECT_LT(0, location_bar_view->height());
}

#if defined(OS_MACOSX)
// https://crbug.com/828031
#define MAYBE_ShowPageInfoDialog DISABLED_ShowPageInfoDialog
#else
#define MAYBE_ShowPageInfoDialog ShowPageInfoDialog
#endif

IN_PROC_BROWSER_TEST_F(PresentationReceiverWindowViewBrowserTest,
                       MAYBE_ShowPageInfoDialog) {
  content::NavigationController::LoadURLParams load_params(GURL("about:blank"));
  fake_delegate_->web_contents()->GetController().LoadURLWithParams(
      load_params);
  static_cast<PresentationReceiverWindow*>(receiver_view_)
      ->ShowInactiveFullscreen();
  static_cast<ExclusiveAccessContext*>(receiver_view_)->ExitFullscreen();
  ASSERT_FALSE(
      static_cast<ExclusiveAccessContext*>(receiver_view_)->IsFullscreen());

  auto* location_icon_view =
      receiver_view_->location_bar_view()->location_icon_view();
  gfx::Rect local_bounds = location_icon_view->GetLocalBounds();
  gfx::Point local_icon_center(local_bounds.x() + local_bounds.width() / 2,
                               local_bounds.y() + local_bounds.height() / 2);
  ui::MouseEvent security_chip_press_event(
      ui::ET_MOUSE_PRESSED, local_icon_center, local_icon_center,
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent security_chip_release_event(
      ui::ET_MOUSE_RELEASED, local_icon_center, local_icon_center,
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

  location_icon_view->OnMousePressed(security_chip_press_event);
  location_icon_view->OnMouseReleased(security_chip_release_event);
  EXPECT_TRUE(location_icon_view->IsBubbleShowing());
}

}  // namespace
