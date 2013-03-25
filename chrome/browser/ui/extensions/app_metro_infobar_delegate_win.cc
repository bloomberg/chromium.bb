// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/app_metro_infobar_delegate_win.h"

#include "apps/app_launch_for_metro_restart_win.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/metro_chrome_win.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "win8/util/win8_util.h"

namespace chrome {

// static
void AppMetroInfoBarDelegateWin::Create(
    Profile* profile, Mode mode, const std::string& extension_id) {
  DCHECK(win8::IsSingleWindowMetroMode());
  DCHECK_EQ(mode == SHOW_APP_LIST, extension_id.empty());

  // Chrome should never get here via the Ash desktop, so only look for browsers
  // on the native desktop.
  Browser* browser = FindOrCreateTabbedBrowser(
      profile, chrome::HOST_DESKTOP_TYPE_NATIVE);

  // Create a new tab at about:blank, and add the infobar.
  content::OpenURLParams params(
      GURL(chrome::kAboutBlankURL),
      content::Referrer(),
      NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser->OpenURL(params);
  InfoBarService* info_bar_service =
      InfoBarService::FromWebContents(web_contents);
  info_bar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new AppMetroInfoBarDelegateWin(info_bar_service, mode, extension_id)));

  // Use PostTask because we can get here in a COM SendMessage, and
  // ActivateApplication can not be sent nested (returns error
  // RPC_E_CANTCALLOUT_ININPUTSYNCCALL).
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(chrome::ActivateMetroChrome)));
}

AppMetroInfoBarDelegateWin::AppMetroInfoBarDelegateWin(
    InfoBarService* info_bar_service,
    Mode mode,
    const std::string& extension_id)
    : ConfirmInfoBarDelegate(info_bar_service),
      mode_(mode),
      extension_id_(extension_id) {
  DCHECK_EQ(mode_ == SHOW_APP_LIST, extension_id_.empty());
}

AppMetroInfoBarDelegateWin::~AppMetroInfoBarDelegateWin() {}

gfx::Image* AppMetroInfoBarDelegateWin::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(IDR_APP_LIST);
}

string16 AppMetroInfoBarDelegateWin::GetMessageText() const {
  return l10n_util::GetStringUTF16(mode_ == SHOW_APP_LIST ?
      IDS_WIN8_INFOBAR_DESKTOP_RESTART_FOR_APP_LIST :
      IDS_WIN8_INFOBAR_DESKTOP_RESTART_FOR_PACKAGED_APP);
}

int AppMetroInfoBarDelegateWin::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 AppMetroInfoBarDelegateWin::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(button == BUTTON_CANCEL ?
      IDS_WIN8_INFOBAR_DESKTOP_RESTART_TO_LAUNCH_APPS_NO_BUTTON :
      IDS_WIN8_INFOBAR_DESKTOP_RESTART_TO_LAUNCH_APPS_YES_BUTTON);
}

bool AppMetroInfoBarDelegateWin::Accept() {
  PrefService* prefs = g_browser_process->local_state();
  content::WebContents* web_contents = owner()->GetWebContents();
  if (mode_ == SHOW_APP_LIST) {
    prefs->SetBoolean(prefs::kRestartWithAppList, true);
  } else {
    apps::SetAppLaunchForMetroRestart(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()),
        extension_id_);
  }

  web_contents->Close();  // Note: deletes |this|.
  chrome::AttemptRestartWithModeSwitch();
  return false;
}

bool AppMetroInfoBarDelegateWin::Cancel() {
  owner()->GetWebContents()->Close();
  return false;
}

string16 AppMetroInfoBarDelegateWin::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool AppMetroInfoBarDelegateWin::LinkClicked(
    WindowOpenDisposition disposition) {
  content::OpenURLParams params(
      GURL("https://support.google.com/chrome/?p=ib_redirect_to_desktop"),
      content::Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false);
  owner()->GetWebContents()->OpenURL(params);
  return false;
}

}  // namespace chrome
