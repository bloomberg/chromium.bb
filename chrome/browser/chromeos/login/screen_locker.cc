// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker.h"

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/screen_lock_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "cros/chromeos_wm_ipc_enums.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/screen.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

namespace {
// The maxium times that the screen locker should try to grab input,
// and its interval. It has to be able to grab all inputs in 30 seconds,
// otherwise chromium process fails and the session is terminated.
const int64 kRetryGrabIntervalMs = 500;
const int kGrabFailureLimit = 60;
// Each keyboard layout has a dummy input method ID which starts with "xkb:".
const char kValidInputMethodPrefix[] = "xkb:";

// A idle time to show the screen saver in seconds.
const int kScreenSaverIdleTimeout = 15;

// Observer to start ScreenLocker when the screen lock
class ScreenLockObserver : public chromeos::ScreenLockLibrary::Observer,
                           public NotificationObserver {
 public:
  ScreenLockObserver() {
    registrar_.Add(this, NotificationType::LOGIN_USER_CHANGED,
                   NotificationService::AllSources());
  }

  // NotificationObserver overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::LOGIN_USER_CHANGED) {
      // Register Screen Lock after login screen to make sure
      // we don't show the screen lock on top of the login screen by accident.
      if (chromeos::CrosLibrary::Get()->EnsureLoaded())
        chromeos::CrosLibrary::Get()->GetScreenLockLibrary()->AddObserver(this);
    }
  }

  virtual void LockScreen(chromeos::ScreenLockLibrary* obj) {
    LOG(INFO) << "In: ScreenLockObserver::LockScreen";
    SetupInputMethodsForScreenLocker();
    chromeos::ScreenLocker::Show();
  }

  virtual void UnlockScreen(chromeos::ScreenLockLibrary* obj) {
    RestoreInputMethods();
    chromeos::ScreenLocker::Hide();
  }

  virtual void UnlockScreenFailed(chromeos::ScreenLockLibrary* obj) {
    chromeos::ScreenLocker::UnlockScreenFailed();
  }

 private:
  // Temporarily deactivates all input methods (e.g. Chinese, Japanese, Arabic)
  // since they are not necessary to input a login password. Users are still
  // able to use/switch active keyboard layouts (e.g. US qwerty, US dvorak,
  // French).
  void SetupInputMethodsForScreenLocker() {
    if (chromeos::CrosLibrary::Get()->EnsureLoaded() &&
        // The LockScreen function is also called when the OS is suspended, and
        // in that case |saved_active_input_method_list_| might be non-empty.
        saved_active_input_method_list_.empty()) {
      chromeos::InputMethodLibrary* language =
          chromeos::CrosLibrary::Get()->GetInputMethodLibrary();
      chromeos::KeyboardLibrary* keyboard =
          chromeos::CrosLibrary::Get()->GetKeyboardLibrary();

      saved_previous_input_method_id_ = language->previous_input_method().id;
      saved_current_input_method_id_ = language->current_input_method().id;
      scoped_ptr<chromeos::InputMethodDescriptors> active_input_method_list(
          language->GetActiveInputMethods());

      const std::string hardware_keyboard =
          keyboard->GetHardwareKeyboardLayoutName();  // e.g. "xkb:us::eng"
      // We'll add the hardware keyboard if it's not included in
      // |active_input_method_list| so that the user can always use the hardware
      // keyboard on the screen locker.
      bool should_add_hardware_keyboard = true;

      chromeos::ImeConfigValue value;
      value.type = chromeos::ImeConfigValue::kValueTypeStringList;
      for (size_t i = 0; i < active_input_method_list->size(); ++i) {
        const std::string& input_method_id = active_input_method_list->at(i).id;
        saved_active_input_method_list_.push_back(input_method_id);
        // |active_input_method_list| contains both input method descriptions
        // and keyboard layout descriptions.
        if (!StartsWithASCII(input_method_id, kValidInputMethodPrefix, true))
          continue;
        value.string_list_value.push_back(input_method_id);
        if (input_method_id == hardware_keyboard) {
          should_add_hardware_keyboard = false;
        }
      }
      if (should_add_hardware_keyboard) {
        value.string_list_value.push_back(hardware_keyboard);
      }
      language->SetImeConfig(
          chromeos::language_prefs::kGeneralSectionName,
          chromeos::language_prefs::kPreloadEnginesConfigName,
          value);
    }
  }

  void RestoreInputMethods() {
    if (chromeos::CrosLibrary::Get()->EnsureLoaded() &&
        !saved_active_input_method_list_.empty()) {
      chromeos::InputMethodLibrary* language =
          chromeos::CrosLibrary::Get()->GetInputMethodLibrary();

      chromeos::ImeConfigValue value;
      value.type = chromeos::ImeConfigValue::kValueTypeStringList;
      value.string_list_value = saved_active_input_method_list_;
      language->SetImeConfig(
          chromeos::language_prefs::kGeneralSectionName,
          chromeos::language_prefs::kPreloadEnginesConfigName,
          value);
      // Send previous input method id first so Ctrl+space would work fine.
      if (!saved_previous_input_method_id_.empty())
        language->ChangeInputMethod(saved_previous_input_method_id_);
      if (!saved_current_input_method_id_.empty())
        language->ChangeInputMethod(saved_current_input_method_id_);

      saved_previous_input_method_id_.clear();
      saved_current_input_method_id_.clear();
      saved_active_input_method_list_.clear();
    }
  }

  NotificationRegistrar registrar_;
  std::string saved_previous_input_method_id_;
  std::string saved_current_input_method_id_;
  std::vector<std::string> saved_active_input_method_list_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockObserver);
};

// A ScreenLock window that covers entire screen to keeps the keyboard
// focus/events inside the grab widget.
class LockWindow : public views::WidgetGtk {
 public:
  LockWindow()
      : WidgetGtk(views::WidgetGtk::TYPE_WINDOW),
        toplevel_focus_widget_(NULL) {
    EnableDoubleBuffer(true);
  }

  // GTK propagates key events from parents to children.
  // Make sure LockWindow will never handle key events.
  virtual gboolean OnKeyPress(GtkWidget* widget, GdkEventKey* event) {
    // Don't handle key event in the lock window.
    return false;
  }

  virtual gboolean OnKeyRelease(GtkWidget* widget, GdkEventKey* event) {
    // Don't handle key event in the lock window.
    return false;
  }

  virtual void OnDestroy(GtkWidget* object) {
    LOG(INFO) << "OnDestroy: LockWindow destroyed";
    views::WidgetGtk::OnDestroy(object);
  }

  virtual void ClearNativeFocus() {
    DCHECK(toplevel_focus_widget_);
    gtk_widget_grab_focus(toplevel_focus_widget_);
  }

  // Sets the widget to move the focus to when clearning the native
  // widget's focus.
  void set_toplevel_focus_widget(GtkWidget* widget) {
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);
    toplevel_focus_widget_ = widget;
  }

 private:
  // The widget we set focus to when clearning the focus on native
  // widget.  In screen locker, gdk input is grabbed in GrabWidget,
  // and resetting the focus by using gtk_window_set_focus seems to
  // confuse gtk and doesn't let focus move to native widget under
  // GrabWidget.
  GtkWidget* toplevel_focus_widget_;

  DISALLOW_COPY_AND_ASSIGN(LockWindow);
};

// A child widget that grabs both keyboard and pointer input.
class GrabWidget : public views::WidgetGtk {
 public:
  explicit GrabWidget(chromeos::ScreenLocker* screen_locker)
      : views::WidgetGtk(views::WidgetGtk::TYPE_CHILD),
        screen_locker_(screen_locker),
        ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
        grab_failure_count_(0),
        kbd_grab_status_(GDK_GRAB_INVALID_TIME),
        mouse_grab_status_(GDK_GRAB_INVALID_TIME) {
  }

  virtual void Show() {
    views::WidgetGtk::Show();
    // Now steal all inputs.
    TryGrabAllInputs();
  }

  void ClearGrab() {
    GtkWidget* current_grab_window;
    // Grab gtk input first so that the menu holding grab will close itself.
    gtk_grab_add(window_contents());

    // Make sure there is no grab widget so that gtk simply propagates
    // an event.  This is necessary to allow message bubble and password
    // field, button to process events simultaneously. GTK
    // maintains grab widgets in a linked-list, so we need to remove
    // until it's empty.
    while ((current_grab_window = gtk_grab_get_current()) != NULL)
      gtk_grab_remove(current_grab_window);
  }

  virtual gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event) {
    WidgetGtk::OnButtonPress(widget, event);
    // Never propagate event to parent.
    return true;
  }

  // Try to grab all inputs. It initiates another try if it fails to
  // grab and the retry count is within a limit, or fails with CHECK.
  void TryGrabAllInputs();

 private:
  virtual void HandleGrabBroke() {
    // Input should never be stolen from ScreenLocker once it's
    // grabbed.  If this happens, it's a bug and has to be fixed. We
    // let chrome crash to get a crash report and dump, and
    // SessionManager will terminate the session to logout.
    CHECK(kbd_grab_status_ != GDK_GRAB_SUCCESS ||
          mouse_grab_status_ != GDK_GRAB_SUCCESS)
        << "Grab Broke. quitting";
  }

  chromeos::ScreenLocker* screen_locker_;
  ScopedRunnableMethodFactory<GrabWidget> task_factory_;

  // The number times the widget tried to grab all focus.
  int grab_failure_count_;
  // Status of keyboard and mouse grab.
  GdkGrabStatus kbd_grab_status_;
  GdkGrabStatus mouse_grab_status_;

  DISALLOW_COPY_AND_ASSIGN(GrabWidget);
};

void GrabWidget::TryGrabAllInputs() {
  ClearGrab();

  if (kbd_grab_status_ != GDK_GRAB_SUCCESS) {
    kbd_grab_status_ = gdk_keyboard_grab(window_contents()->window, FALSE,
                                         GDK_CURRENT_TIME);
  }
  if (mouse_grab_status_ != GDK_GRAB_SUCCESS) {
    mouse_grab_status_ =
        gdk_pointer_grab(window_contents()->window,
                         FALSE,
                         static_cast<GdkEventMask>(
                             GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK),
                         NULL,
                         NULL,
                         GDK_CURRENT_TIME);
  }
  if ((kbd_grab_status_ != GDK_GRAB_SUCCESS ||
       mouse_grab_status_ != GDK_GRAB_SUCCESS) &&
      grab_failure_count_++ < kGrabFailureLimit) {
    LOG(WARNING) << "Failed to grab inputs. Trying again in 1 second: kbd="
        << kbd_grab_status_ << ", mouse=" << mouse_grab_status_;
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        task_factory_.NewRunnableMethod(&GrabWidget::TryGrabAllInputs),
        kRetryGrabIntervalMs);
  } else {
    CHECK_EQ(GDK_GRAB_SUCCESS, kbd_grab_status_)
        << "Failed to grab keyboard input:" << kbd_grab_status_;
    CHECK_EQ(GDK_GRAB_SUCCESS, mouse_grab_status_)
        << "Failed to grab pointer input:" << mouse_grab_status_;
    DLOG(INFO) << "Grab Success";
    screen_locker_->OnGrabInputs();
  }
}

// BackgroundView for ScreenLocker, which layouts a lock widget in
// addition to other background components.
class ScreenLockerBackgroundView : public chromeos::BackgroundView {
 public:
  explicit ScreenLockerBackgroundView(views::WidgetGtk* lock_widget)
      : lock_widget_(lock_widget) {
  }

  virtual bool IsScreenLockerMode() const {
    return true;
  }

  virtual void Layout() {
    chromeos::BackgroundView::Layout();
    gfx::Rect screen = bounds();
    gfx::Rect size;
    lock_widget_->GetBounds(&size, false);
    lock_widget_->SetBounds(
        gfx::Rect((screen.width() - size.width()) / 2,
                  (screen.height() - size.height()) / 2,
                  size.width(),
                  size.height()));
  }

 private:
  views::WidgetGtk* lock_widget_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerBackgroundView);
};

}  // namespace

namespace chromeos {

// static
ScreenLocker* ScreenLocker::screen_locker_ = NULL;

// A event observer that forwards gtk events from one window to another.
// See screen_locker.h for more details.
class MouseEventRelay : public MessageLoopForUI::Observer {
 public:
  MouseEventRelay(GdkWindow* src, GdkWindow* dest)
      : src_(src),
        dest_(dest),
        initialized_(false) {
    DCHECK(src_);
    DCHECK(dest_);
  }

  virtual void WillProcessEvent(GdkEvent* event) {}

  virtual void DidProcessEvent(GdkEvent* event) {
    if (event->any.window != src_)
      return;
    if (!initialized_) {
      gint src_x, src_y, dest_x, dest_y, width, height, depth;
      gdk_window_get_geometry(dest_, &dest_x, &dest_y, &width, &height, &depth);
      // wait to compute offset until the info bubble widget's location
      // is available.
      if (dest_x < 0 || dest_y < 0)
        return;
      gdk_window_get_geometry(src_, &src_x, &src_y, &width, &height, &depth);
      offset_.SetPoint(dest_x - src_x, dest_y - src_y);
      initialized_ = true;
    }
    if (event->type == GDK_BUTTON_PRESS ||
        event->type == GDK_BUTTON_RELEASE) {
      GdkEvent* copy = gdk_event_copy(event);
      copy->button.window = dest_;
      g_object_ref(copy->button.window);
      copy->button.x -= offset_.x();
      copy->button.y -= offset_.y();

      gdk_event_put(copy);
      gdk_event_free(copy);
    } else if (event->type == GDK_MOTION_NOTIFY) {
      GdkEvent* copy = gdk_event_copy(event);
      copy->motion.window = dest_;
      g_object_ref(copy->motion.window);
      copy->motion.x -= offset_.x();
      copy->motion.y -= offset_.y();

      gdk_event_put(copy);
      gdk_event_free(copy);
    }
  }

 private:
  GdkWindow* src_;
  GdkWindow* dest_;
  bool initialized_;

  // Offset from src_'s origin to dest_'s origin.
  gfx::Point offset_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventRelay);
};

// A event observer used to unlock the screen upon user's action
// without asking password. Used in BWSI and auto login mode.
// TODO(oshima): consolidate InputEventObserver and LockerInputEventObserver.
class InputEventObserver : public MessageLoopForUI::Observer {
 public:
  explicit InputEventObserver(ScreenLocker* screen_locker)
      : screen_locker_(screen_locker),
        activated_(false) {
  }

  virtual void WillProcessEvent(GdkEvent* event) {
    if ((event->type == GDK_KEY_PRESS ||
         event->type == GDK_BUTTON_PRESS ||
         event->type == GDK_MOTION_NOTIFY) &&
        !activated_) {
      activated_ = true;
      std::string not_used_string;
      GaiaAuthConsumer::ClientLoginResult not_used;
      screen_locker_->OnLoginSuccess(not_used_string, not_used);
    }
  }

  virtual void DidProcessEvent(GdkEvent* event) {
  }

 private:
  chromeos::ScreenLocker* screen_locker_;

  bool activated_;

  DISALLOW_COPY_AND_ASSIGN(InputEventObserver);
};

// A event observer used to show the screen locker upon
// user action: mouse or keyboard interactions.
// TODO(oshima): this has to be disabled while authenticating.
class LockerInputEventObserver : public MessageLoopForUI::Observer {
 public:
  explicit LockerInputEventObserver(ScreenLocker* screen_locker)
      : screen_locker_(screen_locker),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            timer_(base::TimeDelta::FromSeconds(kScreenSaverIdleTimeout), this,
                   &LockerInputEventObserver::StartScreenSaver)) {
  }

  virtual void WillProcessEvent(GdkEvent* event) {
    if ((event->type == GDK_KEY_PRESS ||
         event->type == GDK_BUTTON_PRESS ||
         event->type == GDK_MOTION_NOTIFY)) {
      timer_.Reset();
      screen_locker_->StopScreenSaver();
    }
  }

  virtual void DidProcessEvent(GdkEvent* event) {
  }

 private:
  void StartScreenSaver() {
    screen_locker_->StartScreenSaver();
  }

  chromeos::ScreenLocker* screen_locker_;
  base::DelayTimer<LockerInputEventObserver> timer_;

  DISALLOW_COPY_AND_ASSIGN(LockerInputEventObserver);
};

//////////////////////////////////////////////////////////////////////////////
// ScreenLocker, public:

ScreenLocker::ScreenLocker(const UserManager::User& user)
    : lock_window_(NULL),
      lock_widget_(NULL),
      screen_lock_view_(NULL),
      user_(user),
      error_info_(NULL),
      drawn_(false),
      input_grabbed_(false),
      // TODO(oshima): support auto login mode (this is not implemented yet)
      // http://crosbug.com/1881
      unlock_on_input_(user_.email().empty()) {
  DCHECK(!screen_locker_);
  screen_locker_ = this;
}

void ScreenLocker::Init() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);

  gfx::Point left_top(1, 1);
  gfx::Rect init_bounds(views::Screen::GetMonitorAreaNearestPoint(left_top));

  LockWindow* lock_window = new LockWindow();
  lock_window_ = lock_window;
  lock_window_->Init(NULL, init_bounds);

  g_signal_connect(lock_window_->GetNativeView(), "client-event",
                   G_CALLBACK(OnClientEventThunk), this);

  // GTK does not like zero width/height.
  gfx::Size size(1, 1);
  if (!unlock_on_input_) {
    screen_lock_view_ = new ScreenLockView(this);
    screen_lock_view_->Init();
    size = screen_lock_view_->GetPreferredSize();
  } else {
    input_event_observer_.reset(new InputEventObserver(this));
    MessageLoopForUI::current()->AddObserver(input_event_observer_.get());
  }

  lock_widget_ = new GrabWidget(this);
  lock_widget_->MakeTransparent();
  lock_widget_->InitWithWidget(lock_window_, gfx::Rect(size));
  if (screen_lock_view_)
    lock_widget_->SetContentsView(screen_lock_view_);
  lock_widget_->GetRootView()->SetVisible(false);
  lock_widget_->Show();

  // Configuring the background url.
  std::string url_string =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kScreenSaverUrl);
  background_view_ = new ScreenLockerBackgroundView(lock_widget_);
  background_view_->Init(GURL(url_string));

  DCHECK(GTK_WIDGET_REALIZED(lock_window_->GetNativeView()));
  WmIpc::instance()->SetWindowType(
      lock_window_->GetNativeView(),
      WM_IPC_WINDOW_CHROME_SCREEN_LOCKER,
      NULL);

  lock_window_->SetContentsView(background_view_);
  lock_window_->Show();

  // Don't let X draw default background, which was causing flash on
  // resume.
  gdk_window_set_back_pixmap(lock_window_->GetNativeView()->window,
                             NULL, false);
  gdk_window_set_back_pixmap(lock_widget_->GetNativeView()->window,
                             NULL, false);
  lock_window->set_toplevel_focus_widget(lock_widget_->window_contents());
}

void ScreenLocker::OnLoginFailure(const LoginFailure& error) {
  DLOG(INFO) << "OnLoginFailure";
  EnableInput();
  // Don't enable signout button here as we're showing
  // MessageBubble.
  gfx::Rect rect = screen_lock_view_->GetPasswordBoundsRelativeTo(
      lock_widget_->GetRootView());
  gfx::Rect lock_widget_bounds;
  lock_widget_->GetBounds(&lock_widget_bounds, false);
  rect.Offset(lock_widget_bounds.x(), lock_widget_bounds.y());

  if (error_info_)
    error_info_->Close();
  std::wstring msg = l10n_util::GetString(IDS_LOGIN_ERROR_AUTHENTICATING);
  const std::string error_text = error.GetErrorString();
  if (!error_text.empty())
    msg += L"\n" + ASCIIToWide(error_text);

  InputMethodLibrary* input_method_library =
      CrosLibrary::Get()->GetInputMethodLibrary();
  if (input_method_library->GetNumActiveInputMethods() > 1)
    msg += L"\n" + l10n_util::GetString(IDS_LOGIN_ERROR_KEYBOARD_SWITCH_HINT);

  error_info_ = MessageBubble::ShowNoGrab(
      lock_window_,
      rect,
      BubbleBorder::BOTTOM_LEFT,
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING),
      msg,
      std::wstring(),  // TODO: add help link
      this);
  if (mouse_event_relay_.get()) {
    MessageLoopForUI::current()->RemoveObserver(mouse_event_relay_.get());
  }
  mouse_event_relay_.reset(
      new MouseEventRelay(lock_widget_->GetNativeView()->window,
                          error_info_->GetNativeView()->window));
  MessageLoopForUI::current()->AddObserver(mouse_event_relay_.get());
}

void ScreenLocker::OnLoginSuccess(const std::string& username,
    const GaiaAuthConsumer::ClientLoginResult& unused) {
  LOG(INFO) << "OnLoginSuccess: Sending Unlock request.";
  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetScreenLockLibrary()->NotifyScreenUnlockRequested();
}

void ScreenLocker::InfoBubbleClosing(InfoBubble* info_bubble,
                                     bool closed_by_escape) {
  error_info_ = NULL;
  screen_lock_view_->SetSignoutEnabled(true);
  if (mouse_event_relay_.get()) {
    MessageLoopForUI::current()->RemoveObserver(mouse_event_relay_.get());
    mouse_event_relay_.reset();
  }
}

void ScreenLocker::Authenticate(const string16& password) {
  screen_lock_view_->SetEnabled(false);
  screen_lock_view_->SetSignoutEnabled(false);
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::AuthenticateToUnlock,
                        user_.email(),
                        UTF16ToUTF8(password)));
}

void ScreenLocker::ClearErrors() {
  if (error_info_) {
    error_info_->Close();
    error_info_ = NULL;
  }
}

void ScreenLocker::EnableInput() {
  if (screen_lock_view_) {
    screen_lock_view_->SetEnabled(true);
    screen_lock_view_->ClearAndSetFocusToPassword();
  }
}

void ScreenLocker::Signout() {
  if (!error_info_) {
    // TODO(oshima): record this action in user metrics.
    if (CrosLibrary::Get()->EnsureLoaded()) {
      CrosLibrary::Get()->GetLoginLibrary()->StopSession("");
    }

    // Don't hide yet the locker because the chrome screen may become visible
    // briefly.
  }
}

void ScreenLocker::OnGrabInputs() {
  DLOG(INFO) << "OnGrabInputs";
  input_grabbed_ = true;
  if (drawn_)
    ScreenLockReady();
}

// static
void ScreenLocker::Show() {
  LOG(INFO) << "In ScreenLocker::Show";
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);

  // Exit fullscreen.
  Browser* browser = BrowserList::GetLastActive();
  // browser can be NULL if we receive a lock request before the first browser
  // window is shown.
  if (browser && browser->window()->IsFullscreen()) {
    browser->ToggleFullscreenMode();
  }

  if (!screen_locker_) {
    LOG(INFO) << "Show: Locking screen";
    ScreenLocker* locker =
        new ScreenLocker(UserManager::Get()->logged_in_user());
    locker->Init();
  } else {
    // PowerManager re-sends lock screen signal if it doesn't
    // receive the response within timeout. Just send complete
    // signal.
    LOG(INFO) << "Show: locker already exists. "
              << "just sending completion event";
    if (CrosLibrary::Get()->EnsureLoaded())
      CrosLibrary::Get()->GetScreenLockLibrary()->NotifyScreenLockCompleted();
  }
}

// static
void ScreenLocker::Hide() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  DCHECK(screen_locker_);
  LOG(INFO) << "Hide: Deleting ScreenLocker:" << screen_locker_;
  MessageLoopForUI::current()->DeleteSoon(FROM_HERE, screen_locker_);
}

// static
void ScreenLocker::UnlockScreenFailed() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  if (screen_locker_) {
    // Power manager decided no to unlock the screen even if a user
    // typed in password, for example, when a user closed the lid
    // immediately after typing in the password.
    LOG(INFO) << "UnlockScreenFailed: re-enabling screen locker";
    screen_locker_->EnableInput();
  } else {
    // This can happen when a user requested unlock, but PowerManager
    // rejected because the computer is closed, then PowerManager unlocked
    // because it's open again and the above failure message arrives.
    // This'd be extremely rare, but may still happen.
    LOG(INFO) << "UnlockScreenFailed: screen is already unlocked.";
  }
}

// static
void ScreenLocker::InitClass() {
  Singleton<ScreenLockObserver>::get();
}

////////////////////////////////////////////////////////////////////////////////
// ScreenLocker, private:

ScreenLocker::~ScreenLocker() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  ClearErrors();
  if (input_event_observer_.get())
    MessageLoopForUI::current()->RemoveObserver(input_event_observer_.get());
  if (locker_input_event_observer_.get()) {
    lock_widget_->GetFocusManager()->UnregisterAccelerator(
        views::Accelerator(app::VKEY_ESCAPE, false, false, false), this);
    MessageLoopForUI::current()->RemoveObserver(
        locker_input_event_observer_.get());
  }

  gdk_keyboard_ungrab(GDK_CURRENT_TIME);
  gdk_pointer_ungrab(GDK_CURRENT_TIME);

  DCHECK(lock_window_);
  LOG(INFO) << "~ScreenLocker(): Closing ScreenLocker window";
  lock_window_->Close();
  // lock_widget_ will be deleted by gtk's destroy signal.
  screen_locker_ = NULL;
  bool state = false;
  NotificationService::current()->Notify(
      NotificationType::SCREEN_LOCK_STATE_CHANGED,
      Source<ScreenLocker>(this),
      Details<bool>(&state));
  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetScreenLockLibrary()->NotifyScreenUnlockCompleted();
}

void ScreenLocker::SetAuthenticator(Authenticator* authenticator) {
  authenticator_ = authenticator;
}

void ScreenLocker::ScreenLockReady() {
  LOG(INFO) << "ScreenLockReady: sending completed signal to power manager.";
  // Don't show the password field until we grab all inputs.
  lock_widget_->GetRootView()->SetVisible(true);
  if (background_view_->ScreenSaverEnabled()) {
    lock_widget_->GetFocusManager()->RegisterAccelerator(
        views::Accelerator(app::VKEY_ESCAPE, false, false, false), this);
    locker_input_event_observer_.reset(new LockerInputEventObserver(this));
    MessageLoopForUI::current()->AddObserver(
        locker_input_event_observer_.get());
    StartScreenSaver();
  } else {
    EnableInput();
  }

  bool state = true;
  NotificationService::current()->Notify(
      NotificationType::SCREEN_LOCK_STATE_CHANGED,
      Source<ScreenLocker>(this),
      Details<bool>(&state));
  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetScreenLockLibrary()->NotifyScreenLockCompleted();
}

void ScreenLocker::OnClientEvent(GtkWidget* widge, GdkEventClient* event) {
  WmIpc::Message msg;
  WmIpc::instance()->DecodeMessage(*event, &msg);
  if (msg.type() == WM_IPC_MESSAGE_CHROME_NOTIFY_SCREEN_REDRAWN_FOR_LOCK) {
    OnWindowManagerReady();
  }
}

void ScreenLocker::OnWindowManagerReady() {
  DLOG(INFO) << "OnClientEvent: drawn for lock";
  drawn_ = true;
  if (input_grabbed_)
    ScreenLockReady();
}

void ScreenLocker::StopScreenSaver() {
  if (background_view_->IsScreenSaverVisible()) {
    LOG(INFO) << "StopScreenSaver";
    background_view_->HideScreenSaver();
    if (screen_lock_view_) {
      screen_lock_view_->SetVisible(true);
      screen_lock_view_->RequestFocus();
    }
    EnableInput();
  }
}

void ScreenLocker::StartScreenSaver() {
  if (!background_view_->IsScreenSaverVisible()) {
    LOG(INFO) << "StartScreenSaver";
    background_view_->ShowScreenSaver();
    if (screen_lock_view_) {
      screen_lock_view_->SetEnabled(false);
      screen_lock_view_->SetVisible(false);
    }
    ClearErrors();
  }
}

bool ScreenLocker::AcceleratorPressed(const views::Accelerator& accelerator) {
  if (!background_view_->IsScreenSaverVisible()) {
    StartScreenSaver();
    return true;
  }
  return false;
}

}  // namespace chromeos
