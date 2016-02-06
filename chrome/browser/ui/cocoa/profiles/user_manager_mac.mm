// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/profiles/user_manager_mac.h"

#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser_dialogs.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#include "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/grit/chromium_strings.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

// Update the App Controller with a new Profile. Used when a Profile is locked
// to set the Controller to the Guest profile so the old Profile's bookmarks,
// etc... cannot be accessed.
void ChangeAppControllerForProfile(Profile* profile,
                                   Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    AppController* controller =
        base::mac::ObjCCast<AppController>([NSApp delegate]);
    [controller windowChangedToProfile:profile];
  }
}

}  // namespace

// An open User Manager window. There can only be one open at a time. This
// is reset to NULL when the window is closed.
UserManagerMac* instance_ = NULL;  // Weak.
BOOL instance_under_construction_ = NO;

void CloseInstanceReauthDialog() {
  DCHECK(instance_);
  instance_->CloseReauthDialog();
}

// The modal dialog host the User Manager uses to display the reauth dialog.
class UserManagerModalHost : public web_modal::WebContentsModalDialogHost {
 public:
  UserManagerModalHost(gfx::NativeView host_view)
      : host_view_(host_view) {}

  gfx::Size GetMaximumDialogSize() override {
    return gfx::Size(
        UserManager::kReauthDialogWidth, UserManager::kReauthDialogHeight);
  }

  ~UserManagerModalHost() override {}

  gfx::NativeView GetHostView() const override {
    return host_view_;
  }

  gfx::Point GetDialogPosition(const gfx::Size& size) override {
    return gfx::Point(0, 0);
  }

  void AddObserver(web_modal::ModalDialogHostObserver* observer) override {}
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override {}

 private:
  gfx::NativeView host_view_;
};

// The modal manager delegate allowing the display of constrained windows for
// the reauth dialog.
class UserManagerModalManagerDelegate :
    public web_modal::WebContentsModalDialogManagerDelegate {
 public:
  UserManagerModalManagerDelegate(gfx::NativeView host_view) {
    modal_host_.reset(new UserManagerModalHost(host_view));
  }

  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override {
    return modal_host_.get();
  }

  bool IsWebContentsVisible(content::WebContents* web_contents) override {
    return true;
  }

   ~UserManagerModalManagerDelegate() override {}
 protected:
  scoped_ptr<UserManagerModalHost> modal_host_;
};

// Custom WebContentsDelegate that allows handling of hotkeys.
class UserManagerWebContentsDelegate : public content::WebContentsDelegate {
 public:
  UserManagerWebContentsDelegate() {}

  // WebContentsDelegate implementation. Forwards all unhandled keyboard events
  // to the current window.
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    if (![BrowserWindowUtils shouldHandleKeyboardEvent:event])
      return;

    // -getCommandId returns -1 if the event isn't a chrome accelerator.
    int chromeCommandId = [BrowserWindowUtils getCommandId:event];

    // Check for Cmd+A and Cmd+V events that could come from a password field.
    bool isTextEditingCommand =
        (event.modifiers & blink::WebInputEvent::MetaKey) &&
        (event.windowsKeyCode == ui::VKEY_A ||
         event.windowsKeyCode == ui::VKEY_V);

    // Only handle close window Chrome accelerators and text editing ones.
    if (chromeCommandId == IDC_CLOSE_WINDOW || chromeCommandId == IDC_EXIT ||
        isTextEditingCommand) {
      [[NSApp mainMenu] performKeyEquivalent:event.os_event];
    }
  }
};

class ReauthDialogDelegate : public UserManager::ReauthDialogObserver,
                             public UserManagerWebContentsDelegate,
                             public ConstrainedWindowMacDelegate {
 public:
  ReauthDialogDelegate(content::WebContents* web_contents,
                       const std::string& email)
      : UserManager::ReauthDialogObserver(web_contents, email) {}

  // UserManager::ReauthDialogObserver:
  void CloseReauthDialog() override {
    CloseInstanceReauthDialog();
  }

  // ConstrainedWindowMacDelegate:
  void OnConstrainedWindowClosed(ConstrainedWindowMac* window) override {
    CloseReauthDialog();
  }

  DISALLOW_COPY_AND_ASSIGN(ReauthDialogDelegate);
};

// WindowController for the reauth dialog.
@interface ReauthDialogWindowController
    : NSWindowController <NSWindowDelegate> {
 @private
  std::string emailAddress_;
  content::WebContents* webContents_;
  scoped_ptr<ReauthDialogDelegate> webContentsDelegate_;
  std::unique_ptr<ConstrainedWindowMac> constrained_window_;
  scoped_ptr<content::WebContents> reauthWebContents_;
}
- (id)initWithProfile:(Profile*)profile
                email:(std::string)email
          webContents:(content::WebContents*)webContents;
- (void)close;
@end

@implementation ReauthDialogWindowController

- (id)initWithProfile:(Profile*)profile
                email:(std::string)email
          webContents:(content::WebContents*)webContents {
  webContents_ = webContents;
  emailAddress_ = email;

  NSRect frame = NSMakeRect(
      0, 0, UserManager::kReauthDialogWidth, UserManager::kReauthDialogHeight);
  base::scoped_nsobject<ConstrainedWindowCustomWindow> window(
      [[ConstrainedWindowCustomWindow alloc]
          initWithContentRect:frame
                    styleMask:NSTitledWindowMask | NSClosableWindowMask]);
  if ((self = [super initWithWindow:window])) {
    webContents_ = webContents;

    reauthWebContents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(profile)));
    window.get().contentView = reauthWebContents_->GetNativeView();
    webContentsDelegate_.reset(
       new ReauthDialogDelegate(reauthWebContents_.get(), emailAddress_));
    reauthWebContents_->SetDelegate(webContentsDelegate_.get());

    base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
       [[CustomConstrainedWindowSheet alloc]
           initWithCustomWindow:[self window]]);
    constrained_window_ =
        CreateAndShowWebModalDialogMac(
            webContentsDelegate_.get(), webContents_, sheet);

    // The close button needs to call CloseWebContentsModalDialog() on the
    // constrained window isntead of just [window close] so grab a reference to
    // it in the title bar and change its action.
    auto closeButton = [window standardWindowButton:NSWindowCloseButton];
    [closeButton setTarget:self];
    [closeButton setAction:@selector(closeButtonClicked:)];
    [self show];
  }

  return self;
}

- (void)show {
  GURL url = signin::GetReauthURLWithEmail(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::Reason::REASON_UNLOCK, emailAddress_);
  reauthWebContents_->GetController().LoadURL(url, content::Referrer(),
                                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
}

- (void)closeButtonClicked:(NSButton*)button {
  [self close];
}

- (void)close {
  constrained_window_->CloseWebContentsModalDialog();
}

- (void)dealloc {
  constrained_window_->CloseWebContentsModalDialog();

  [super dealloc];
}

@end

// Window controller for the User Manager view.
@interface UserManagerWindowController : NSWindowController <NSWindowDelegate> {
 @private
  scoped_ptr<content::WebContents> webContents_;
  scoped_ptr<UserManagerWebContentsDelegate> webContentsDelegate_;
  UserManagerMac* userManagerObserver_;  // Weak.
  scoped_ptr<UserManagerModalManagerDelegate> modal_manager_delegate_;
  base::scoped_nsobject<ReauthDialogWindowController> reauth_window_controller_;
}
- (void)windowWillClose:(NSNotification*)notification;
- (void)dealloc;
- (id)initWithProfile:(Profile*)profile
         withObserver:(UserManagerMac*)userManagerObserver;
- (void)showURL:(const GURL&)url;
- (void)show;
- (void)close;
- (BOOL)isVisible;
- (void)showReauthDialogWithProfile:(Profile*)profile email:(std::string)email;
- (void)closeReauthDialog;
@end

@implementation UserManagerWindowController

- (id)initWithProfile:(Profile*)profile
         withObserver:(UserManagerMac*)userManagerObserver {
  // Center the window on the screen that currently has focus.
  NSScreen* mainScreen = [NSScreen mainScreen];
  CGFloat screenHeight = [mainScreen frame].size.height;
  CGFloat screenWidth = [mainScreen frame].size.width;

  NSRect contentRect =
      NSMakeRect((screenWidth - UserManager::kWindowWidth) / 2,
                 (screenHeight - UserManager::kWindowHeight) / 2,
                 UserManager::kWindowWidth, UserManager::kWindowHeight);
  ChromeEventProcessingWindow* window = [[ChromeEventProcessingWindow alloc]
      initWithContentRect:contentRect
                styleMask:NSTitledWindowMask |
                          NSClosableWindowMask |
                          NSResizableWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO
                   screen:mainScreen];
  [window setTitle:l10n_util::GetNSString(IDS_PRODUCT_NAME)];
  [window setMinSize:NSMakeSize(UserManager::kWindowWidth,
                                UserManager::kWindowHeight)];

  if ((self = [super initWithWindow:window])) {
    userManagerObserver_ = userManagerObserver;

    // Initialize the web view.
    webContents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(profile)));
    window.contentView = webContents_->GetNativeView();
    webContentsDelegate_.reset(new UserManagerWebContentsDelegate());
    webContents_->SetDelegate(webContentsDelegate_.get());

    web_modal::WebContentsModalDialogManager::CreateForWebContents(
        webContents_.get());
    modal_manager_delegate_.reset(
        new UserManagerModalManagerDelegate([[self window] contentView]));
    web_modal::WebContentsModalDialogManager::FromWebContents(
        webContents_.get())->SetDelegate(modal_manager_delegate_.get());

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowWillClose:)
               name:NSWindowWillCloseNotification
             object:self.window];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  // Remove the ModalDailogManager that's about to be destroyed.
  auto manager = web_modal::WebContentsModalDialogManager::FromWebContents(
      webContents_.get());
  if (manager)
    manager->SetDelegate(nullptr);

  [super dealloc];
}

- (void)showURL:(const GURL&)url {
  webContents_->GetController().LoadURL(url, content::Referrer(),
                                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
  content::RenderWidgetHostView* rwhv = webContents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetBackgroundColor(profiles::kUserManagerBackgroundColor);
  [self show];
}

- (void)show {
  // Because the User Manager isn't a BrowserWindowController, activating it
  // will not trigger a -windowChangedToProfile and update the menu bar.
  // This is only important if the active profile is Guest, which may have
  // happened after locking a profile.
  if (profiles::SetActiveProfileToGuestIfLocked()) {
    g_browser_process->profile_manager()->CreateProfileAsync(
        ProfileManager::GetGuestProfilePath(),
        base::Bind(&ChangeAppControllerForProfile),
        base::string16(),
        std::string(),
        std::string());
  }
  [[self window] makeKeyAndOrderFront:self];
}

- (void)close {
  [[self window] close];
}

-(BOOL)isVisible {
  return [[self window] isVisible];
}

- (void)windowWillClose:(NSNotification*)notification {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  DCHECK(userManagerObserver_);
  userManagerObserver_->WindowWasClosed();
}

- (void)showReauthDialogWithProfile:(Profile*)profile email:(std::string)email {
  reauth_window_controller_.reset(
      [[ReauthDialogWindowController alloc]
          initWithProfile:profile
                    email:email
              webContents:webContents_.get()]);
}

- (void)closeReauthDialog {
  [reauth_window_controller_ close];
}

@end


// static
void UserManager::Show(
    const base::FilePath& profile_path_to_focus,
    profiles::UserManagerTutorialMode tutorial_mode,
    profiles::UserManagerProfileSelected profile_open_action) {
  DCHECK(profile_path_to_focus != ProfileManager::GetGuestProfilePath());

  ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::OPEN_USER_MANAGER);
  if (instance_) {
    // If there's a user manager window open already, just activate it.
    [instance_->window_controller() show];
    instance_->set_user_manager_started_showing(base::Time::Now());
    return;
  }

  // Under some startup conditions, we can try twice to create the User Manager.
  // Because creating the System profile is asynchronous, it's possible for
  // there to then be multiple pending operations and eventually multiple
  // User Managers.
  if (instance_under_construction_)
      return;
  instance_under_construction_ = YES;

  // Create the guest profile, if necessary, and open the User Manager
  // from the guest profile.
  profiles::CreateSystemProfileForUserManager(
      profile_path_to_focus,
      tutorial_mode,
      profile_open_action,
      base::Bind(&UserManagerMac::OnSystemProfileCreated, base::Time::Now()));
}

// static
void UserManager::Hide() {
  if (instance_)
    [instance_->window_controller() close];
}

// static
bool UserManager::IsShowing() {
  return instance_ ? [instance_->window_controller() isVisible]: false;
}

// static
void UserManager::OnUserManagerShown() {
  if (instance_)
    instance_->LogTimeToOpen();
}

// static
void UserManager::ShowReauthDialog(content::BrowserContext* browser_context,
                                   const std::string& email) {
  DCHECK(instance_);
  instance_->ShowReauthDialog(browser_context, email);
}

void UserManagerMac::ShowReauthDialog(content::BrowserContext* browser_context,
                                      const std::string& email) {
  [window_controller_
      showReauthDialogWithProfile:Profile::FromBrowserContext(browser_context)
                            email:email];
}

void UserManagerMac::CloseReauthDialog() {
  [window_controller_ closeReauthDialog];
}

UserManagerMac::UserManagerMac(Profile* profile) {
  window_controller_.reset([[UserManagerWindowController alloc]
      initWithProfile:profile withObserver:this]);
}

UserManagerMac::~UserManagerMac() {
}

// static
void UserManagerMac::OnSystemProfileCreated(const base::Time& start_time,
                                            Profile* system_profile,
                                            const std::string& url) {
  DCHECK(!instance_);
  instance_ = new UserManagerMac(system_profile);
  instance_->set_user_manager_started_showing(start_time);
  [instance_->window_controller() showURL:GURL(url)];
  instance_under_construction_ = NO;
}

void UserManagerMac::LogTimeToOpen() {
  if (user_manager_started_showing_ == base::Time())
    return;

  ProfileMetrics::LogTimeToOpenUserManager(
      base::Time::Now() - user_manager_started_showing_);
  user_manager_started_showing_ = base::Time();
}

void UserManagerMac::WindowWasClosed() {
  CloseReauthDialog();
  instance_ = NULL;
  delete this;
}
