// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/session_crashed_prompt.h"

#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// A delegate for the InfoBar shown when the previous session has crashed.
class SessionCrashedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SessionCrashedInfoBarDelegate(Profile* profile,
                                InfoBarTabHelper* infobar_helper);

 private:
  virtual ~SessionCrashedInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // The Profile that we restore sessions from.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SessionCrashedInfoBarDelegate);
};

SessionCrashedInfoBarDelegate::SessionCrashedInfoBarDelegate(
    Profile* profile,
    InfoBarTabHelper* infobar_helper)
    : ConfirmInfoBarDelegate(infobar_helper),
      profile_(profile) {
}

SessionCrashedInfoBarDelegate::~SessionCrashedInfoBarDelegate() {
}

gfx::Image* SessionCrashedInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_RESTORE_SESSION);
}

string16 SessionCrashedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_MESSAGE);
}

int SessionCrashedInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

string16 SessionCrashedInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON);
}

bool SessionCrashedInfoBarDelegate::Accept() {
  uint32 behavior = 0;
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (browser && browser->tab_count() == 1
      && browser->GetWebContentsAt(0)->GetURL() ==
      GURL(chrome::kChromeUINewTabURL)) {
    // There is only one tab and its the new tab page, make session restore
    // clobber it.
    behavior = SessionRestore::CLOBBER_CURRENT_TAB;
  }
  SessionRestore::RestoreSession(
      profile_, browser, behavior, std::vector<GURL>());
  return true;
}

}  // namespace


namespace browser {

void ShowSessionCrashedPrompt(Browser* browser) {
  // Assume that if the user is launching incognito they were previously
  // running incognito so that we have nothing to restore from.
  if (browser->profile()->IsOffTheRecord())
    return;

  // In ChromeBot tests, there might be a race. This line appears to get
  // called during shutdown and |tab| can be NULL.
  TabContentsWrapper* tab = browser->GetSelectedTabContentsWrapper();
  if (!tab)
    return;

  // Don't show the info-bar if there are already info-bars showing.
  if (tab->infobar_tab_helper()->infobar_count() > 0)
    return;

  tab->infobar_tab_helper()->AddInfoBar(
      new SessionCrashedInfoBarDelegate(browser->profile(),
                                        tab->infobar_tab_helper()));
}

}  // namespace browser

