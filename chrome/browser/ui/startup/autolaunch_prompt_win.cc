// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/autolaunch_prompt.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/auto_launch_trial.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"


// AutolaunchInfoBarDelegate --------------------------------------------------

namespace {

// The delegate for the infobar shown when Chrome is auto-launched.
class AutolaunchInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an autolaunch infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service, Profile* profile);

 private:
  explicit AutolaunchInfoBarDelegate(Profile* profile);
  virtual ~AutolaunchInfoBarDelegate();

  void set_should_expire() { should_expire_ = true; }

  // ConfirmInfoBarDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool ShouldExpireInternal(
      const NavigationDetails& details) const OVERRIDE;

  // Weak pointer to the profile, not owned by us.
  Profile* profile_;

  // Whether the info-bar should be dismissed on the next navigation.
  bool should_expire_;

  // Used to delay the expiration of the info-bar.
  base::WeakPtrFactory<AutolaunchInfoBarDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutolaunchInfoBarDelegate);
};

// static
void AutolaunchInfoBarDelegate::Create(InfoBarService* infobar_service,
                                       Profile* profile) {
  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new AutolaunchInfoBarDelegate(profile))));
}

AutolaunchInfoBarDelegate::AutolaunchInfoBarDelegate(
    Profile* profile)
    : ConfirmInfoBarDelegate(),
      profile_(profile),
      should_expire_(false),
      weak_factory_(this) {
  PrefService* prefs = profile->GetPrefs();
  prefs->SetInteger(prefs::kShownAutoLaunchInfobar,
                    prefs->GetInteger(prefs::kShownAutoLaunchInfobar) + 1);

  // We want the info-bar to stick-around for a few seconds and then be hidden
  // on the next navigation after that.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AutolaunchInfoBarDelegate::set_should_expire,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(8));
}

AutolaunchInfoBarDelegate::~AutolaunchInfoBarDelegate() {
}

int AutolaunchInfoBarDelegate::GetIconID() const {
  return IDR_PRODUCT_LOGO_32;
}

base::string16 AutolaunchInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_AUTO_LAUNCH_INFOBAR_TEXT);
}

base::string16 AutolaunchInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_AUTO_LAUNCH_OK : IDS_AUTO_LAUNCH_REVERT);
}

bool AutolaunchInfoBarDelegate::Accept() {
  return true;
}

bool AutolaunchInfoBarDelegate::Cancel() {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&auto_launch_util::DisableForegroundStartAtLogin,
                 profile_->GetPath().BaseName().value()));
  return true;
}

bool AutolaunchInfoBarDelegate::ShouldExpireInternal(
    const NavigationDetails& details) const {
  return should_expire_;
}

}  // namespace


// Functions ------------------------------------------------------------------

namespace chrome {

bool ShowAutolaunchPrompt(Browser* browser) {
  if (!auto_launch_trial::IsInAutoLaunchGroup())
    return false;

  // Only supported on the main profile for now.
  Profile* profile = browser->profile();
  if (profile->GetPath().BaseName() !=
      base::FilePath(base::ASCIIToUTF16(chrome::kInitialProfile))) {
    return false;
  }

  int infobar_shown =
      profile->GetPrefs()->GetInteger(prefs::kShownAutoLaunchInfobar);
  const int kMaxTimesToShowInfoBar = 5;
  if (infobar_shown >= kMaxTimesToShowInfoBar)
    return false;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kAutoLaunchAtStartup) &&
      !first_run::IsChromeFirstRun()) {
    return false;
  }

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  AutolaunchInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents),
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  return true;
}

void RegisterAutolaunchUserPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kShownAutoLaunchInfobar, 0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

}  // namespace chrome
