// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autolaunch_prompt.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/auto_launch_trial.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;

namespace {

static const int kMaxInfobarShown = 5;

// The delegate for the infobar shown when Chrome was auto-launched.
class AutolaunchInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  AutolaunchInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                            PrefService* prefs,
                            Profile* profile);
  virtual ~AutolaunchInfoBarDelegate();

 private:
  void AllowExpiry() { should_expire_ = true; }

  // ConfirmInfoBarDelegate:
  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // The prefs to use.
  PrefService* prefs_;

  // Whether the user clicked one of the buttons.
  bool action_taken_;

  // Whether the info-bar should be dismissed on the next navigation.
  bool should_expire_;

  // Weak pointer to the profile, not owned by us.
  Profile* profile_;

  // Used to delay the expiration of the info-bar.
  base::WeakPtrFactory<AutolaunchInfoBarDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutolaunchInfoBarDelegate);
};

AutolaunchInfoBarDelegate::AutolaunchInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    PrefService* prefs,
    Profile* profile)
    : ConfirmInfoBarDelegate(infobar_helper),
      prefs_(prefs),
      action_taken_(false),
      should_expire_(false),
      profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  auto_launch_trial::UpdateInfobarShownMetric();

  int count = prefs_->GetInteger(prefs::kShownAutoLaunchInfobar);
  prefs_->SetInteger(prefs::kShownAutoLaunchInfobar, count + 1);

  // We want the info-bar to stick-around for a few seconds and then be hidden
  // on the next navigation after that.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AutolaunchInfoBarDelegate::AllowExpiry,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(8));
}

AutolaunchInfoBarDelegate::~AutolaunchInfoBarDelegate() {
  if (!action_taken_) {
    auto_launch_trial::UpdateInfobarResponseMetric(
        auto_launch_trial::INFOBAR_IGNORE);
  }
}

bool AutolaunchInfoBarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  return details.is_navigation_to_different_page() && should_expire_;
}

gfx::Image* AutolaunchInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_PRODUCT_LOGO_32);
}

string16 AutolaunchInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_AUTO_LAUNCH_INFOBAR_TEXT);
}

string16 AutolaunchInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_AUTO_LAUNCH_OK : IDS_AUTO_LAUNCH_REVERT);
}

bool AutolaunchInfoBarDelegate::Accept() {
  action_taken_ = true;
  auto_launch_trial::UpdateInfobarResponseMetric(
      auto_launch_trial::INFOBAR_OK);
  return true;
}

bool AutolaunchInfoBarDelegate::Cancel() {
  action_taken_ = true;

  // Track infobar reponse.
  auto_launch_trial::UpdateInfobarResponseMetric(
      auto_launch_trial::INFOBAR_CUT_IT_OUT);
  // Also make sure we keep track of how many disable and how many enable.
  auto_launch_trial::UpdateToggleAutoLaunchMetric(false);

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&auto_launch_util::DisableForegroundStartAtLogin,
                 profile_->GetPath().BaseName().value()));
  return true;
}

void CheckAutoLaunchCallback(Profile* profile) {
  if (!auto_launch_trial::IsInAutoLaunchGroup())
    return;

  // We must not use GetLastActive here because this is at Chrome startup and
  // no window might have been made active yet. We'll settle for any window.
  Browser* browser = BrowserList::FindAnyBrowser(profile, true);
  TabContentsWrapper* tab = browser->GetSelectedTabContentsWrapper();

  // Don't show the info-bar if there are already info-bars showing.
  InfoBarTabHelper* infobar_helper = tab->infobar_tab_helper();
  if (infobar_helper->infobar_count() > 0)
    return;

  infobar_helper->AddInfoBar(
      new AutolaunchInfoBarDelegate(infobar_helper,
      tab->profile()->GetPrefs(), tab->profile()));
}

}  // namespace

namespace browser {

bool ShowAutolaunchPrompt(Profile* profile) {
  if (!auto_launch_trial::IsInAutoLaunchGroup())
    return false;

  // Only supported on the main profile for now.
  if (profile->GetPath().BaseName().value() !=
      ASCIIToUTF16(chrome::kInitialProfile)) {
    return false;
  }

  int infobar_shown =
      profile->GetPrefs()->GetInteger(prefs::kShownAutoLaunchInfobar);
  if (infobar_shown >= kMaxInfobarShown)
    return false;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kChromeFrame))
    return false;

  if (command_line.HasSwitch(switches::kAutoLaunchAtStartup) ||
      first_run::IsChromeFirstRun()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&CheckAutoLaunchCallback, profile));
    return true;
  }
  return false;
}

void RegisterAutolaunchPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(
      prefs::kShownAutoLaunchInfobar, 0, PrefService::UNSYNCABLE_PREF);
}

}  // namespace browser

