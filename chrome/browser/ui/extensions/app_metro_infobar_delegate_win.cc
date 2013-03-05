// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/app_metro_infobar_delegate_win.h"

#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
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

void AppMetroInfoBarDelegateWin::CreateAndActivateMetro(Profile* profile) {
  // Chrome should never get here via the Ash desktop, so only look for browsers
  // on the native desktop.
  CHECK(win8::IsSingleWindowMetroMode());
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
      new AppMetroInfoBarDelegateWin(info_bar_service)));

  // Use PostTask because we can get here in a COM SendMessage, and
  // ActivateApplication can not be sent nested (returns error
  // RPC_E_CANTCALLOUT_ININPUTSYNCCALL).
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(chrome::ActivateMetroChrome)));
}

AppMetroInfoBarDelegateWin::AppMetroInfoBarDelegateWin(
    InfoBarService* info_bar_service)
    : ConfirmInfoBarDelegate(info_bar_service) {
}

AppMetroInfoBarDelegateWin::~AppMetroInfoBarDelegateWin() {}

gfx::Image* AppMetroInfoBarDelegateWin::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(IDR_APP_LIST);
}

string16 AppMetroInfoBarDelegateWin::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_WIN8_INFOBAR_DESKTOP_RESTART_TO_LAUNCH_APPS);
}

int AppMetroInfoBarDelegateWin::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 AppMetroInfoBarDelegateWin::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_CANCEL) {
    return l10n_util::GetStringUTF16(
        IDS_WIN8_INFOBAR_DESKTOP_RESTART_TO_LAUNCH_APPS_NO_BUTTON);
  }

  return l10n_util::GetStringUTF16(
      IDS_WIN8_INFOBAR_DESKTOP_RESTART_TO_LAUNCH_APPS_YES_BUTTON);
}

bool AppMetroInfoBarDelegateWin::Accept() {
  owner()->GetWebContents()->Close();
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kRestartWithAppList, true);
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
