// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include <gdk/gdk.h>
#include <signal.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "app/resource_bundle.h"
#include "app/l10n_util.h"
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

// Passing this parameter as a "first screen" initiates full OOBE flow.
const char kOutOfBoxScreenName[] = "oobe";

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


// Returns bounds of the screen to use for login wizard.
// The rect is centered within the default monitor and sized accordingly if
// |size| is not empty. Otherwise the whole monitor is occupied.
gfx::Rect CalculateScreenBounds(const gfx::Size& size) {
  gfx::Rect bounds(views::Screen::GetMonitorWorkAreaNearestWindow(NULL));
  if (!size.IsEmpty()) {
    int horizontal_diff = bounds.width() - size.width();
    int vertical_diff = bounds.height() - size.height();
    bounds.Inset(horizontal_diff / 2, vertical_diff / 2);
  }

  return bounds;
}

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
      current_screen_(NULL),
      is_out_of_box_(false) {
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
                            const gfx::Rect& screen_bounds,
                            bool paint_background) {
  DCHECK(!contents_);

  int offset_x = (screen_bounds.width() - kWizardScreenWidth) / 2;
  int offset_y = (screen_bounds.height() - kWizardScreenHeight) / 2;
  int window_x = screen_bounds.x() + offset_x;
  int window_y = screen_bounds.y() + offset_y;

  contents_ = new ContentView(paint_background,
                              offset_x, offset_y,
                              screen_bounds.width(), screen_bounds.height());

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

  if (chromeos::UserManager::Get()->GetUsers().empty() ||
      first_screen_name == kOutOfBoxScreenName) {
    is_out_of_box_ = true;
  }

  ShowFirstScreen(first_screen_name);

  // This keeps the window from flashing at startup.
  GdkWindow* gdk_window = window->GetNativeView()->window;
  gdk_window_set_back_pixmap(gdk_window, NULL, false);
}

void WizardController::Show() {
  DCHECK(widget_);
  widget_->Show();
}

void WizardController::ShowBackground(const gfx::Rect& bounds) {
  DCHECK(!background_widget_);
  background_widget_ =
      chromeos::BackgroundView::CreateWindowContainingView(bounds,
                                                           &background_view_);
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

chromeos::AccountScreen* WizardController::GetAccountScreen() {
  if (!account_screen_.get())
    account_screen_.reset(new chromeos::AccountScreen(this));
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
  // We're on the stack, so don't try and delete us now.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WizardController::OnLoginCreateAccount() {
  SetCurrentScreen(GetAccountScreen());
}

void WizardController::OnNetworkConnected() {
  if (is_out_of_box_) {
    SetCurrentScreen(GetUpdateScreen());
    update_screen_->StartUpdate();
  } else {
    SetCurrentScreen(GetLoginScreen());
  }
}

void WizardController::OnNetworkOffline() {
  // TODO(dpolukhin): if(is_out_of_box_) we cannot work offline and
  // should report some error message here and stay on the same screen.
  SetCurrentScreen(GetLoginScreen());
}

void WizardController::OnAccountCreateBack() {
  SetCurrentScreen(GetLoginScreen());
}

void WizardController::OnAccountCreated() {
  LoginScreen* login = GetLoginScreen();
  SetCurrentScreen(login);
  if (!username_.empty()) {
    login->view()->SetUsername(username_);
    if (!password_.empty()) {
      login->view()->SetPassword(password_);
      // TODO(dpolukhin): clear password memory for real. Now it is not
      // a problem becuase we can't extract password from the form.
      password_.clear();
      login->view()->Login();
    }
  }
}

void WizardController::OnConnectionFailed() {
  // TODO(dpolukhin): show error message before going back to network screen.
  is_out_of_box_ = false;
  SetCurrentScreen(GetNetworkScreen());
}

void WizardController::OnLanguageChanged() {
  SetCurrentScreen(GetNetworkScreen());
}

void WizardController::OnUpdateCompleted() {
  SetCurrentScreen(GetLoginScreen());
}

void WizardController::OnUpdateNetworkError() {
  // If network connection got interrupted while downloading the update,
  // return to network selection screen.
  SetCurrentScreen(GetNetworkScreen());
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, private:
void WizardController::OnSwitchLanguage(const std::string& lang) {
  // Delete all views that may reference locale-specific data.
  SetCurrentScreen(NULL);
  network_screen_.reset();
  login_screen_.reset();
  account_screen_.reset();
  update_screen_.reset();
  contents_->RemoveAllChildViews(true);
  // Can't delete background view since we don't know how to recreate it.
  if (background_view_)
    background_view_->Teardown();

  // Switch the locale.
  ResourceBundle::CleanupSharedInstance();
  ResourceBundle::InitSharedInstance(UTF8ToWide(lang));

  // The following line does not seem to affect locale anyhow. Maybe in future..
  g_browser_process->SetApplicationLocale(lang);

  // Recreate view hierarchy and return to the wizard screen.
  if (background_view_)
    background_view_->Init();
  OnExit(chromeos::ScreenObserver::LANGUAGE_CHANGED);
}

void WizardController::OnSetUserNamePassword(const std::string& username,
                                             const std::string& password) {
  username_ = username;
  password_ = password;
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
    update_screen_->StartUpdate();
  } else {
    if (is_out_of_box_) {
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
      break;
    case NETWORK_CONNECTED:
      OnNetworkConnected();
      break;
    case NETWORK_OFFLINE:
      OnNetworkOffline();
      break;
    case ACCOUNT_CREATE_BACK:
      OnAccountCreateBack();
      break;
    case ACCOUNT_CREATED:
      OnAccountCreated();
      break;
    case CONNECTION_FAILED:
      OnConnectionFailed();
      break;
    case LANGUAGE_CHANGED:
      OnLanguageChanged();
      break;
    case UPDATE_INSTALLED:
    case UPDATE_NOUPDATE:
      OnUpdateCompleted();
      break;
    case UPDATE_NETWORK_ERROR:
      OnUpdateNetworkError();
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
  gfx::Rect screen_bounds(CalculateScreenBounds(size));

  if (first_screen_name.empty() &&
      chromeos::CrosLibrary::Get()->EnsureLoaded() &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLoginImages)) {
    std::vector<chromeos::UserManager::User> users =
        chromeos::UserManager::Get()->GetUsers();
    if (!users.empty()) {
      // ExistingUserController deletes itself.
      (new chromeos::ExistingUserController(users, screen_bounds))->Init();
      return;
    }
  }

  WizardController* controller = new WizardController();
  controller->ShowBackground(screen_bounds);
  controller->Init(first_screen_name, screen_bounds, true);
  controller->Show();
  if (chromeos::CrosLibrary::Get()->EnsureLoaded())
    chromeos::CrosLibrary::Get()->GetLoginLibrary()->EmitLoginPromptReady();
}

}  // namespace browser
