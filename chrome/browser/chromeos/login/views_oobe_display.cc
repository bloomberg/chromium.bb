// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/enrollment/views_enterprise_enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/views_eula_screen_actor.h"
#include "chrome/browser/chromeos/login/views_network_screen_actor.h"
#include "chrome/browser/chromeos/login/views_oobe_display.h"
#include "chrome/browser/chromeos/login/views_update_screen_actor.h"
#include "chrome/browser/chromeos/login/views_user_image_screen_actor.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"
#include "views/view.h"
#include "views/widget/widget.h"

namespace chromeos {

namespace {

// RootView of the Widget WizardController creates. Contains the contents of the
// WizardController.
class ContentView : public views::View {
 public:
  ContentView()
      : accel_toggle_accessibility_(
            chromeos::WizardAccessibilityHelper::GetAccelerator()) {
#if defined(OFFICIAL_BUILD)
    accel_cancel_update_ =  views::Accelerator(ui::VKEY_ESCAPE,
                                               true, true, true);
#else
    accel_cancel_update_ =  views::Accelerator(ui::VKEY_ESCAPE,
                                               false, false, false);
    accel_login_screen_ = views::Accelerator(ui::VKEY_L,
                                             false, true, true);
    accel_network_screen_ = views::Accelerator(ui::VKEY_N,
                                               false, true, true);
    accel_update_screen_ = views::Accelerator(ui::VKEY_U,
                                              false, true, true);
    accel_image_screen_ = views::Accelerator(ui::VKEY_I,
                                             false, true, true);
    accel_eula_screen_ = views::Accelerator(ui::VKEY_E,
                                            false, true, true);
    accel_register_screen_ = views::Accelerator(ui::VKEY_R,
                                                false, true, true);
    accel_enterprise_enrollment_screen_ =
        views::Accelerator(ui::VKEY_P, false, true, true);
    AddAccelerator(accel_login_screen_);
    AddAccelerator(accel_network_screen_);
    AddAccelerator(accel_update_screen_);
    AddAccelerator(accel_image_screen_);
    AddAccelerator(accel_eula_screen_);
    AddAccelerator(accel_register_screen_);
    AddAccelerator(accel_enterprise_enrollment_screen_);
#endif
    AddAccelerator(accel_toggle_accessibility_);
    AddAccelerator(accel_cancel_update_);
  }

  ~ContentView() {
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_WIZARD_CONTENT_VIEW_DESTROYED,
        NotificationService::AllSources(),
        NotificationService::NoDetails());
  }

  bool AcceleratorPressed(const views::Accelerator& accel) {
    WizardController* controller = WizardController::default_controller();
    if (!controller)
      return false;

    if (accel == accel_toggle_accessibility_) {
      chromeos::WizardAccessibilityHelper::GetInstance()->ToggleAccessibility();
    } else if (accel == accel_cancel_update_) {
      controller->CancelOOBEUpdate();
#if !defined(OFFICIAL_BUILD)
    } else if (accel == accel_login_screen_) {
      controller->ShowLoginScreen();
    } else if (accel == accel_network_screen_) {
      controller->ShowNetworkScreen();
    } else if (accel == accel_update_screen_) {
      controller->ShowUpdateScreen();
    } else if (accel == accel_image_screen_) {
      controller->ShowUserImageScreen();
    } else if (accel == accel_eula_screen_) {
      controller->ShowEulaScreen();
    } else if (accel == accel_register_screen_) {
      controller->ShowRegistrationScreen();
    } else if (accel == accel_enterprise_enrollment_screen_) {
      controller->ShowEnterpriseEnrollmentScreen();
#endif
    } else {
      return false;
    }

    return true;
  }

  virtual void Layout() {
    for (int i = 0; i < child_count(); ++i) {
      views::View* cur = child_at(i);
      if (cur->IsVisible())
        cur->SetBounds(0, 0, width(), height());
    }
  }

 private:
#if !defined(OFFICIAL_BUILD)
  views::Accelerator accel_login_screen_;
  views::Accelerator accel_network_screen_;
  views::Accelerator accel_update_screen_;
  views::Accelerator accel_image_screen_;
  views::Accelerator accel_eula_screen_;
  views::Accelerator accel_register_screen_;
  views::Accelerator accel_enterprise_enrollment_screen_;
#endif
  views::Accelerator accel_toggle_accessibility_;
  views::Accelerator accel_cancel_update_;

  DISALLOW_COPY_AND_ASSIGN(ContentView);
};

}  // namespace

ViewsOobeDisplay::ViewsOobeDisplay(const gfx::Rect& screen_bounds)
    : widget_(NULL),
      contents_(new ContentView()),
      screen_bounds_(screen_bounds),
      initial_show_(true),
      screen_observer_(NULL) {
}

ViewsOobeDisplay::~ViewsOobeDisplay() {
  if (widget_) {
    widget_->Close();
    widget_ = NULL;
  }
}

void ViewsOobeDisplay::ShowScreen(WizardScreen* screen) {
  bool force_widget_show = false;
  views::Widget* window = NULL;

  gfx::Rect current_bounds;
  if (widget_)
    current_bounds = widget_->GetClientAreaScreenBounds();

  // Calls SetScreenSize() method.
  screen->PrepareToShow();

  gfx::Rect new_bounds = GetWizardScreenBounds(screen_size_.width(),
                                               screen_size_.height());
  if (new_bounds != current_bounds) {
    if (widget_)
      widget_->Close();
    force_widget_show = true;
    window = CreateScreenWindow(new_bounds, initial_show_);
  }
  screen->Show();
  contents_->Layout();
  contents_->SchedulePaint();
  if (force_widget_show) {
    // This keeps the window from flashing at startup.
    GdkWindow* gdk_window = window->GetNativeView()->window;
    gdk_window_set_back_pixmap(gdk_window, NULL, false);
    if (widget_)
      widget_->Show();
  }
}

void ViewsOobeDisplay::HideScreen(WizardScreen* screen) {
  initial_show_ = false;
  screen->Hide();
  contents_->Layout();
  contents_->SchedulePaint();
}

UpdateScreenActor* ViewsOobeDisplay::GetUpdateScreenActor() {
  if (update_screen_actor_ == NULL)
    update_screen_actor_.reset(new ViewsUpdateScreenActor(this));
  return update_screen_actor_.get();
}

NetworkScreenActor* ViewsOobeDisplay::GetNetworkScreenActor() {
  if (network_screen_actor_ == NULL)
    network_screen_actor_.reset(new ViewsNetworkScreenActor(this));
  return network_screen_actor_.get();
}

EulaScreenActor* ViewsOobeDisplay::GetEulaScreenActor() {
  if (eula_screen_actor_ == NULL)
    eula_screen_actor_.reset(new ViewsEulaScreenActor(this));
  return eula_screen_actor_.get();
}

EnterpriseEnrollmentScreenActor* ViewsOobeDisplay::
    GetEnterpriseEnrollmentScreenActor() {
  if (enterprise_enrollment_screen_actor_ == NULL) {
    enterprise_enrollment_screen_actor_.reset(
        new ViewsEnterpriseEnrollmentScreenActor(this));
  }
  return enterprise_enrollment_screen_actor_.get();
}

UserImageScreenActor* ViewsOobeDisplay::GetUserImageScreenActor() {
  if (user_image_screen_actor_ == NULL)
    user_image_screen_actor_.reset(new ViewsUserImageScreenActor(this));
  return user_image_screen_actor_.get();
}

ViewScreenDelegate* ViewsOobeDisplay::GetRegistrationScreenActor() {
  return this;
}

ViewScreenDelegate* ViewsOobeDisplay::GetHTMLPageScreenActor() {
  return this;
}

views::Widget* ViewsOobeDisplay::CreateScreenWindow(
    const gfx::Rect& bounds, bool initial_show) {
  widget_ = new views::Widget;
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  // Window transparency makes background flicker through controls that
  // are constantly updating its contents (like image view with video
  // stream). Hence enabling double buffer.
  widget_params.double_buffer = true;
  widget_params.transparent = true;
  widget_params.bounds = bounds;
  widget_->Init(widget_params);
  std::vector<int> params;
  // For initial show WM would animate background window.
  // Otherwise it stays unchanged.
  params.push_back(initial_show);
  chromeos::WmIpc::instance()->SetWindowType(
      widget_->GetNativeView(),
      chromeos::WM_IPC_WINDOW_LOGIN_GUEST,
      &params);
  widget_->SetContentsView(contents_);
  return widget_;
}

gfx::Rect ViewsOobeDisplay::GetWizardScreenBounds(int screen_width,
                                                  int screen_height) const {
  int offset_x = (screen_bounds_.width() - screen_width) / 2;
  int offset_y = (screen_bounds_.height() - screen_height) / 2;
  int window_x = screen_bounds_.x() + offset_x;
  int window_y = screen_bounds_.y() + offset_y;
  return gfx::Rect(window_x, window_y, screen_width, screen_height);
}

views::View* ViewsOobeDisplay::GetWizardView() {
  return contents_;
}

chromeos::ScreenObserver* ViewsOobeDisplay::GetObserver() {
  return screen_observer_;
}

void ViewsOobeDisplay::SetScreenSize(const gfx::Size &screen_size) {
  screen_size_ = screen_size;
}

void ViewsOobeDisplay::SetScreenObserver(ScreenObserver* screen_observer) {
  screen_observer_ = screen_observer;
}

}  // namespace chromeos
