// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include <string>
#include <vector>

#include "app/gfx/canvas.h"
#include "base/logging.h"  // For NOTREACHED.
#include "chrome/browser/chromeos/login/account_screen.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "views/view.h"
#include "views/window/window.h"

namespace {

const int kWizardScreenWidth = 512;
const int kWizardScreenHeight = 416;

const char kNetworkScreenName[] = "network";
const char kLoginScreenName[] = "login";
const char kAccountScreenName[] = "account";

}  // namespace

// Contents view for wizard's window. Parents screen views and status area
// view.
class WizardContentsView : public views::View {
 public:
  explicit WizardContentsView(chromeos::StatusAreaView* status_area)
      : status_area_(status_area) {
  }
  ~WizardContentsView() {}

  void Init() {
    views::Painter* painter = chromeos::CreateWizardPainter(
        &chromeos::BorderDefinition::kWizardBorder);
    set_background(views::Background::CreateBackgroundPainter(true, painter));
    AddChildView(status_area_);
  }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() {
    return size();
  }

  virtual void Layout() {
    int right_top_padding =
        chromeos::BorderDefinition::kWizardBorder.padding +
        chromeos::BorderDefinition::kWizardBorder.corner_radius / 2;
    gfx::Size status_area_size = status_area_->GetPreferredSize();
    status_area_->SetBounds(
        width() - status_area_size.width() - right_top_padding,
        right_top_padding,
        status_area_size.width(),
        status_area_size.height());

    // Layout screen view. It should be the only visible child that's not a
    // status area view.
    for (int i = 0; i < GetChildViewCount(); ++i) {
      views::View* cur = GetChildViewAt(i);
      if (cur != status_area_ && cur->IsVisible()) {
        int x = (width() - kWizardScreenWidth) / 2;
        int y = (height() - kWizardScreenHeight) / 2;
        cur->SetBounds(x, y, kWizardScreenWidth, kWizardScreenHeight);
      }
    }
  }

 private:
  chromeos::StatusAreaView* status_area_;

  DISALLOW_COPY_AND_ASSIGN(WizardContentsView);
};

///////////////////////////////////////////////////////////////////////////////
// WizardController, public:
WizardController::WizardController()
    : contents_(NULL),
      status_area_(NULL),
      current_screen_(NULL) {
}

WizardController::~WizardController() {
}

void WizardController::ShowFirstScreen(const std::string& first_screen_name) {
  if (first_screen_name == kNetworkScreenName) {
    SetCurrentScreen(GetNetworkScreen());
  } else if (first_screen_name == kLoginScreenName) {
    SetCurrentScreen(GetLoginScreen());
  } else if (first_screen_name == kAccountScreenName) {
    SetCurrentScreen(GetAccountScreen());
  } else {
    if (chromeos::UserManager::Get()->GetUsers().empty()) {
      SetCurrentScreen(GetNetworkScreen());
    } else {
      SetCurrentScreen(GetLoginScreen());
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, ExitHandlers:
void WizardController::OnLoginSignInSelected() {
  window()->Close();
}

void WizardController::OnLoginCreateAccount() {
  SetCurrentScreen(GetAccountScreen());
}

void WizardController::OnNetworkConnected() {
  SetCurrentScreen(GetLoginScreen());
}

void WizardController::OnAccountCreated() {
  // GetLoginScreen()->SetUsername(GetAccountScreen()->GetUsername());
  SetCurrentScreen(GetLoginScreen());
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, private:
void WizardController::InitContents() {
  status_area_ = new chromeos::StatusAreaView(this);
  status_area_->Init();

  contents_ = new WizardContentsView(status_area_);
  contents_->Init();
}

NetworkScreen* WizardController::GetNetworkScreen() {
  if (!network_screen_.get())
    network_screen_.reset(new NetworkScreen(this));
  return network_screen_.get();
}

LoginScreen* WizardController::GetLoginScreen() {
  if (!login_screen_.get())
    login_screen_.reset(new LoginScreen(this));
  return login_screen_.get();
}

AccountScreen* WizardController::GetAccountScreen() {
  if (!account_screen_.get())
    account_screen_.reset(new AccountScreen(this));
  return account_screen_.get();
}

void WizardController::SetCurrentScreen(WizardScreen* new_current) {
  if (current_screen_)
    current_screen_->Hide();
  current_screen_ = new_current;
  if (current_screen_)
    current_screen_->Show();
  contents_->Layout();
  contents_->SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, chromeos::ScreenObserver overrides:
void WizardController::OnExit(ExitCodes exit_code) {
  switch (exit_code) {
    case LOGIN_SIGN_IN_SELECTED:
      OnLoginSignInSelected();
      break;
    case LOGIN_CREATE_ACCOUNT:
      OnLoginCreateAccount();
    case NETWORK_CONNECTED:
    case NETWORK_OFFLINE:
      OnNetworkConnected();
      break;
    case ACCOUNT_CREATED:
      OnAccountCreated();
      break;
    default:
      NOTREACHED();
  }
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, views::WindowDelegate overrides:
views::View* WizardController::GetContentsView() {
  if (!contents_)
    InitContents();
  return contents_;
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, StatusAreaHost overrides:
gfx::NativeWindow WizardController::GetNativeWindow() const {
  return window()->GetNativeWindow();
}

bool WizardController::ShouldOpenButtonOptions(
    const views::View* button_view) const {
  if (button_view == status_area_->clock_view()) {
    return false;
  }
  return true;
}

void WizardController::OpenButtonOptions(const views::View* button_view) const {
  // TODO(avayvod): Add some dialog for options or remove them completely.
}

bool WizardController::IsButtonVisible(const views::View* button_view) const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, WizardScreen overrides:
views::View* WizardController::GetWizardView() {
  return contents_;
}

views::Window* WizardController::GetWizardWindow() {
  return window();
}

chromeos::ScreenObserver* WizardController::GetObserver(WizardScreen* screen) {
  return this;
}
