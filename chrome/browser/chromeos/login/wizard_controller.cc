// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include <gdk/gdk.h>
#include <signal.h>
#include <sys/types.h>

#include <string>

#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/logging.h"  // For NOTREACHED.
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/login/account_screen.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/common/chrome_switches.h"
#include "unicode/locid.h"
#include "views/painter.h"
#include "views/screen.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"

namespace {

const int kWizardScreenWidth = 700;
const int kWizardScreenHeight = 416;

const char kNetworkScreenName[] = "network";
const char kLoginScreenName[] = "login";
const char kAccountScreenName[] = "account";
const char kUpdateScreenName[] = "update";

// RootView of the Widget WizardController creates. Contains the contents of the
// WizardController.
class ContentView : public views::View {
 public:
  ContentView(bool paint_background, int window_x, int window_y, int screen_w,
              int screen_h)
      : window_x_(window_x),
        window_y_(window_y),
        screen_w_(screen_w),
        screen_h_(screen_h) {
    if (paint_background) {
      painter_.reset(chromeos::CreateWizardPainter(
                         &chromeos::BorderDefinition::kWizardBorder));
    }
  }

  void PaintBackground(gfx::Canvas* canvas) {
    if (painter_.get()) {
      // TODO(sky): nuke this once new login manager is in place. This needs to
      // exist because with no window manager transparency isn't really
      // supported.
      canvas->TranslateInt(-window_x_, -window_y_);
      painter_->Paint(screen_w_, screen_h_, canvas);
    }
  }

  virtual void Layout() {
    for (int i = 0; i < GetChildViewCount(); ++i) {
      views::View* cur = GetChildViewAt(i);
      if (cur->IsVisible())
        cur->SetBounds(0, 0, width(), height());
    }
  }

 private:
  scoped_ptr<views::Painter> painter_;

  const int window_x_;
  const int window_y_;
  const int screen_w_;
  const int screen_h_;

  DISALLOW_COPY_AND_ASSIGN(ContentView);
};

}  // namespace

// Initialize default controller.
// static
WizardController* WizardController::default_controller_ = NULL;

///////////////////////////////////////////////////////////////////////////////
// WizardController, public:
WizardController::WizardController()
    : widget_(NULL),
      background_widget_(NULL),
      background_view_(NULL),
      contents_(NULL),
      current_screen_(NULL) {
  DCHECK(default_controller_ == NULL);
  default_controller_ = this;
}

WizardController::~WizardController() {
  // Close ends up deleting the widget.
  if (background_widget_)
    background_widget_->Close();

  if (widget_)
    widget_->Close();

  default_controller_ = NULL;
}

void WizardController::Init(const std::string& first_screen_name,
                            bool paint_background) {
  DCHECK(!contents_);

  gfx::Rect screen_bounds =
      views::Screen::GetMonitorWorkAreaNearestWindow(NULL);
  int window_x = (screen_bounds.width() - kWizardScreenWidth) / 2;
  int window_y = (screen_bounds.height() - kWizardScreenHeight) / 2;

  contents_ = new ContentView(paint_background, window_x, window_y,
                              screen_bounds.width(),
                              screen_bounds.height());

  views::WidgetGtk* window =
      new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  widget_ = window;
  if (!paint_background)
    window->MakeTransparent();
  window->Init(NULL, gfx::Rect(window_x, window_y, kWizardScreenWidth,
                               kWizardScreenHeight));
  chromeos::WmIpc::instance()->SetWindowType(
      window->GetNativeView(),
      chromeos::WmIpc::WINDOW_TYPE_LOGIN_GUEST,
      NULL);
  window->SetContentsView(contents_);

  ShowFirstScreen(first_screen_name);

  // This keeps the window from flashing at startup.
  GdkWindow* gdk_window = window->GetNativeView()->window;
  gdk_window_set_back_pixmap(gdk_window, NULL, false);
}

void WizardController::Show() {
  DCHECK(widget_);
  widget_->Show();
}

void WizardController::ShowBackground(const gfx::Size& size) {
  DCHECK(!background_widget_);
  background_widget_ = chromeos::BackgroundView::CreateWindowContainingView(
      gfx::Rect(0, 0, size.width(), size.height()), &background_view_);
  background_widget_->Show();
}

void WizardController::OwnBackground(
    views::Widget* background_widget,
    chromeos::BackgroundView* background_view) {
  DCHECK(!background_widget_);
  background_widget_ = background_widget;
  background_view_ = background_view;
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

UpdateScreen* WizardController::GetUpdateScreen() {
  if (!update_screen_.get())
    update_screen_.reset(new UpdateScreen(this));
  return update_screen_.get();
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, ExitHandlers:
void WizardController::OnLoginSignInSelected() {
  // Close the windows now (which will delete them).
  if (background_widget_) {
    background_widget_->Close();
    background_widget_ = NULL;
  }

  widget_->Close();
  widget_ = NULL;

  // We're on the stack, so don't try and delete us now.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WizardController::OnLoginCreateAccount() {
  SetCurrentScreen(GetAccountScreen());
}

void WizardController::OnNetworkConnected() {
  SetCurrentScreen(GetUpdateScreen());
}

void WizardController::OnAccountCreated() {
  SetCurrentScreen(GetLoginScreen());
}

void WizardController::OnLanguageChanged() {
  SetCurrentScreen(GetNetworkScreen());
}

void WizardController::OnUpdateCompleted() {
  SetCurrentScreen(GetLoginScreen());
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, private:
void WizardController::OnSwitchLanguage(std::string lang) {
  // Delete all views that may may reference locale-specific data.
  SetCurrentScreen(NULL);
  network_screen_.reset();
  login_screen_.reset();
  account_screen_.reset();
  update_screen_.reset();
  contents_->RemoveAllChildViews(true);

  // Switch the locale.
  ResourceBundle::CleanupSharedInstance();
  icu::Locale icu_locale(lang.c_str());
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(icu_locale, error_code);
  DCHECK(U_SUCCESS(error_code));
  ResourceBundle::InitSharedInstance(UTF8ToWide(lang));

  // The following line does not seem to affect locale anyhow. Maybe in future..
  g_browser_process->SetApplicationLocale(lang);

  // Recreate view hierarchy and return to the wizard screen.
  if (background_view_)
    background_view_->RecreateStatusArea();
  OnExit(chromeos::ScreenObserver::LANGUAGE_CHANGED);
}

void WizardController::SetCurrentScreen(WizardScreen* new_current) {
  if (current_screen_)
    current_screen_->Hide();
  current_screen_ = new_current;
  if (current_screen_) {
    current_screen_->Show();
    contents_->Layout();
  }
  contents_->SchedulePaint();
}

void WizardController::ShowFirstScreen(const std::string& first_screen_name) {
  if (first_screen_name == kNetworkScreenName) {
    SetCurrentScreen(GetNetworkScreen());
  } else if (first_screen_name == kLoginScreenName) {
    SetCurrentScreen(GetLoginScreen());
  } else if (first_screen_name == kAccountScreenName) {
    SetCurrentScreen(GetAccountScreen());
  } else if (first_screen_name == kUpdateScreenName) {
    SetCurrentScreen(GetUpdateScreen());
  } else {
    if (chromeos::UserManager::Get()->GetUsers().empty()) {
      SetCurrentScreen(GetNetworkScreen());
    } else {
      SetCurrentScreen(GetLoginScreen());
    }
  }
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
    case LANGUAGE_CHANGED:
      OnLanguageChanged();
      break;
    case UPDATE_INSTALLED:
    case UPDATE_NOUPDATE:
      OnUpdateCompleted();
      break;
    default:
      NOTREACHED();
  }
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, WizardScreen overrides:
views::View* WizardController::GetWizardView() {
  return contents_;
}

chromeos::ScreenObserver* WizardController::GetObserver(WizardScreen* screen) {
  return this;
}

namespace browser {

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
void ShowLoginWizard(const std::string& first_screen_name,
                     const gfx::Size& size) {
  if (first_screen_name.empty() && chromeos::CrosLibrary::EnsureLoaded() &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLoginImages)) {
    std::vector<chromeos::UserManager::User> users =
        chromeos::UserManager::Get()->GetUsers();
    if (!users.empty()) {
      // ExistingUserController deletes itself.
      (new chromeos::ExistingUserController(users, size))->Init();
      return;
    }
  }
  WizardController* controller = new WizardController();
  controller->ShowBackground(size);
  controller->Init(first_screen_name, true);
  controller->Show();
  if (chromeos::CrosLibrary::EnsureLoaded())
    chromeos::LoginLibrary::Get()->EmitLoginPromptReady();
}

}  // namespace browser
