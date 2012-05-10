// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt.h"

#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/pref_names.h"
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

// The delegate for the infobar shown when Chrome is not the default browser.
class DefaultBrowserInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit DefaultBrowserInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                         PrefService* prefs);

 private:
  virtual ~DefaultBrowserInfoBarDelegate();

  void AllowExpiry() { should_expire_ = true; }

  // ConfirmInfoBarDelegate:
  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool NeedElevation(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // The prefs to use.
  PrefService* prefs_;

  // Whether the user clicked one of the buttons.
  bool action_taken_;

  // Whether the info-bar should be dismissed on the next navigation.
  bool should_expire_;

  // Used to delay the expiration of the info-bar.
  base::WeakPtrFactory<DefaultBrowserInfoBarDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DefaultBrowserInfoBarDelegate);
};

DefaultBrowserInfoBarDelegate::DefaultBrowserInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    PrefService* prefs)
    : ConfirmInfoBarDelegate(infobar_helper),
      prefs_(prefs),
      action_taken_(false),
      should_expire_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  // We want the info-bar to stick-around for few seconds and then be hidden
  // on the next navigation after that.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&DefaultBrowserInfoBarDelegate::AllowExpiry,
                            weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(8));
}

DefaultBrowserInfoBarDelegate::~DefaultBrowserInfoBarDelegate() {
  if (!action_taken_)
    UMA_HISTOGRAM_COUNTS("DefaultBrowserWarning.Ignored", 1);
}

bool DefaultBrowserInfoBarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  return details.is_navigation_to_different_page() && should_expire_;
}

gfx::Image* DefaultBrowserInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
     IDR_PRODUCT_LOGO_32);
}

string16 DefaultBrowserInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_SHORT_TEXT);
}

string16 DefaultBrowserInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_SET_AS_DEFAULT_INFOBAR_BUTTON_LABEL :
      IDS_DONT_ASK_AGAIN_INFOBAR_BUTTON_LABEL);
}

bool DefaultBrowserInfoBarDelegate::NeedElevation(InfoBarButton button) const {
  return button == BUTTON_OK;
}

bool DefaultBrowserInfoBarDelegate::Accept() {
  action_taken_ = true;
  UMA_HISTOGRAM_COUNTS("DefaultBrowserWarning.SetAsDefault", 1);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&ShellIntegration::SetAsDefaultBrowser)));
  return true;
}

bool DefaultBrowserInfoBarDelegate::Cancel() {
  action_taken_ = true;
  UMA_HISTOGRAM_COUNTS("DefaultBrowserWarning.DontSetAsDefault", 1);
  // User clicked "Don't ask me again", remember that.
  prefs_->SetBoolean(prefs::kCheckDefaultBrowser, false);
  return true;
}

void NotifyNotDefaultBrowserCallback() {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;  // Reached during ui tests.

  // In ChromeBot tests, there might be a race. This line appears to get
  // called during shutdown and |tab| can be NULL.
  TabContentsWrapper* tab = browser->GetSelectedTabContentsWrapper();
  if (!tab)
    return;

  // Don't show the info-bar if there are already info-bars showing.
  InfoBarTabHelper* infobar_helper = tab->infobar_tab_helper();
  if (infobar_helper->infobar_count() > 0)
    return;

  infobar_helper->AddInfoBar(
      new DefaultBrowserInfoBarDelegate(infobar_helper,
                                        tab->profile()->GetPrefs()));
}

void CheckDefaultBrowserCallback() {
  if (ShellIntegration::IsDefaultBrowser() ||
      !ShellIntegration::CanSetAsDefaultBrowser()) {
    return;
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&NotifyNotDefaultBrowserCallback));
}

}  // namespace

namespace browser {

void ShowDefaultBrowserPrompt(Profile* profile) {
  // We do not check if we are the default browser if:
  // - the user said "don't ask me again" on the infobar earlier.
  // - this is the first launch after the first run flow.
  // - There is a policy in control of this setting.
  if (!profile->GetPrefs()->GetBoolean(prefs::kCheckDefaultBrowser) ||
      first_run::IsChromeFirstRun()) {
    return;
  }
  if (g_browser_process->local_state()->IsManagedPreference(
      prefs::kDefaultBrowserSettingEnabled)) {
    if (g_browser_process->local_state()->GetBoolean(
        prefs::kDefaultBrowserSettingEnabled)) {
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(
              base::IgnoreResult(&ShellIntegration::SetAsDefaultBrowser)));
    } else {
      // TODO(pastarmovj): We can't really do anything meaningful here yet but
      // just prevent showing the infobar.
    }
    return;
  }
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&CheckDefaultBrowserCallback));

}

}  // namespace browser
