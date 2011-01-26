// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/keystone_infobar.h"

#import <AppKit/AppKit.h>

#include <string>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/keystone_glue.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

class SkBitmap;

namespace {

// KeystonePromotionInfoBarDelegate -------------------------------------------

class KeystonePromotionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit KeystonePromotionInfoBarDelegate(TabContents* tab_contents);

 private:
  virtual ~KeystonePromotionInfoBarDelegate();

  // Sets this info bar to be able to expire.  Called a predetermined amount
  // of time after this object is created.
  void SetCanExpire() { can_expire_ = true; }

  // ConfirmInfoBarDelegate
  virtual bool ShouldExpire(
      const NavigationController::LoadCommittedDetails& details) const;
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual string16 GetMessageText() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();

  // The TabContents' profile.
  Profile* profile_;  // weak

  // Whether the info bar should be dismissed on the next navigation.
  bool can_expire_;

  // Used to delay the expiration of the info bar.
  ScopedRunnableMethodFactory<KeystonePromotionInfoBarDelegate> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(KeystonePromotionInfoBarDelegate);
};

KeystonePromotionInfoBarDelegate::KeystonePromotionInfoBarDelegate(
    TabContents* tab_contents)
    : ConfirmInfoBarDelegate(tab_contents),
      profile_(tab_contents->profile()),
      can_expire_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  const int kCanExpireOnNavigationAfterMilliseconds = 8 * 1000;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &KeystonePromotionInfoBarDelegate::SetCanExpire),
      kCanExpireOnNavigationAfterMilliseconds);
}

KeystonePromotionInfoBarDelegate::~KeystonePromotionInfoBarDelegate() {
}

bool KeystonePromotionInfoBarDelegate::ShouldExpire(
    const NavigationController::LoadCommittedDetails& details) const {
  return can_expire_;
}

void KeystonePromotionInfoBarDelegate::InfoBarClosed() {
  delete this;
}

SkBitmap* KeystonePromotionInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_PRODUCT_ICON_32);
}

string16 KeystonePromotionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_PROMOTE_INFOBAR_TEXT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
}

string16 KeystonePromotionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PROMOTE_INFOBAR_PROMOTE_BUTTON : IDS_PROMOTE_INFOBAR_DONT_ASK_BUTTON);
}

bool KeystonePromotionInfoBarDelegate::Accept() {
  [[KeystoneGlue defaultKeystoneGlue] promoteTicket];
  return true;
}

bool KeystonePromotionInfoBarDelegate::Cancel() {
  profile_->GetPrefs()->SetBoolean(prefs::kShowUpdatePromotionInfoBar, false);
  return true;
}

}  // namespace


// KeystonePromotionInfoBar ---------------------------------------------------

@interface KeystonePromotionInfoBar : NSObject
- (void)checkAndShowInfoBarForProfile:(Profile*)profile;
- (void)updateStatus:(NSNotification*)notification;
- (void)removeObserver;
@end  // @interface KeystonePromotionInfoBar

@implementation KeystonePromotionInfoBar

- (void)dealloc {
  [self removeObserver];
  [super dealloc];
}

- (void)checkAndShowInfoBarForProfile:(Profile*)profile {
  // If this is the first run, the user clicked the "don't ask again" button
  // at some point in the past, or if the "don't ask about the default
  // browser" command-line switch is present, bail out.  That command-line
  // switch is recycled here because it's likely that the set of users that
  // don't want to be nagged about the default browser also don't want to be
  // nagged about the update check.  (Automated testers, I'm thinking of
  // you...)
  CommandLine* commandLine = CommandLine::ForCurrentProcess();
  if (FirstRun::IsChromeFirstRun() ||
      !profile->GetPrefs()->GetBoolean(prefs::kShowUpdatePromotionInfoBar) ||
      commandLine->HasSwitch(switches::kNoDefaultBrowserCheck)) {
    return;
  }

  // If there is no Keystone glue (maybe because this application isn't
  // Keystone-enabled) or the application is on a read-only filesystem,
  // doing anything related to auto-update is pointless.  Bail out.
  KeystoneGlue* keystoneGlue = [KeystoneGlue defaultKeystoneGlue];
  if (!keystoneGlue || [keystoneGlue isOnReadOnlyFilesystem]) {
    return;
  }

  // Stay alive as long as needed.  This is balanced by a release in
  // -updateStatus:.
  [self retain];

  AutoupdateStatus recentStatus = [keystoneGlue recentStatus];
  if (recentStatus == kAutoupdateNone ||
      recentStatus == kAutoupdateRegistering) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(updateStatus:)
                   name:kAutoupdateStatusNotification
                 object:nil];
  } else {
    [self updateStatus:[keystoneGlue recentNotification]];
  }
}

- (void)updateStatus:(NSNotification*)notification {
  NSDictionary* dictionary = [notification userInfo];
  AutoupdateStatus status = static_cast<AutoupdateStatus>(
      [[dictionary objectForKey:kAutoupdateStatusStatus] intValue]);

  if (status == kAutoupdateNone || status == kAutoupdateRegistering) {
    return;
  }

  [self removeObserver];

  if (status != kAutoupdateRegisterFailed &&
      [[KeystoneGlue defaultKeystoneGlue] needsPromotion]) {
    Browser* browser = BrowserList::GetLastActive();
    if (browser) {
      TabContents* tabContents = browser->GetSelectedTabContents();

      // Only show if no other info bars are showing, because that's how the
      // default browser info bar works.
      if (tabContents && tabContents->infobar_delegate_count() == 0) {
        tabContents->AddInfoBar(
            new KeystonePromotionInfoBarDelegate(tabContents));
      }
    }
  }

  [self release];
}

- (void)removeObserver {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

@end  // @implementation KeystonePromotionInfoBar

// static
void KeystoneInfoBar::PromotionInfoBar(Profile* profile) {
  KeystonePromotionInfoBar* promotionInfoBar =
      [[[KeystonePromotionInfoBar alloc] init] autorelease];

  [promotionInfoBar checkAndShowInfoBarForProfile:profile];
}
