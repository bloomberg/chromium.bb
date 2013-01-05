// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/session_crashed_prompt.h"

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// A delegate for the InfoBar shown when the previous session has crashed.
class SessionCrashedInfoBarDelegate : public ConfirmInfoBarDelegate,
                                      public content::NotificationObserver {
 public:
  SessionCrashedInfoBarDelegate(InfoBarService* infobar_service,
                                Browser* browser);

 private:
  virtual ~SessionCrashedInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
  bool accepted_;
  bool removed_notification_received_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(SessionCrashedInfoBarDelegate);
};

SessionCrashedInfoBarDelegate::SessionCrashedInfoBarDelegate(
    InfoBarService* infobar_service,
    Browser* browser)
    : ConfirmInfoBarDelegate(infobar_service),
      accepted_(false),
      removed_notification_received_(false),
      browser_(browser) {
  // TODO(pkasting,marja): Once InfoBars own they delegates, this is not needed
  // any more. Then we can rely on delegates getting destroyed, and we can
  // initiate the session storage scavenging only in the destructor. (Currently,
  // info bars are leaked if they get closed while they're in background tabs.)
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                 content::NotificationService::AllSources());
}

SessionCrashedInfoBarDelegate::~SessionCrashedInfoBarDelegate() {
  // If the info bar wasn't accepted, it was either dismissed or expired. In
  // that case, session restore won't happen.
  if (!accepted_ && !removed_notification_received_) {
    content::BrowserContext::GetDefaultStoragePartition(browser_->profile())->
        GetDOMStorageContext()->StartScavengingUnusedSessionStorage();
  }
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
  if (browser_->tab_count() == 1 &&
      chrome::GetWebContentsAt(browser_, 0)->GetURL() ==
          GURL(chrome::kChromeUINewTabURL)) {
    // There is only one tab and its the new tab page, make session restore
    // clobber it.
    behavior = SessionRestore::CLOBBER_CURRENT_TAB;
  }
  SessionRestore::RestoreSession(
      browser_->profile(), browser_, behavior, std::vector<GURL>());
  accepted_ = true;
  return true;
}

void SessionCrashedInfoBarDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED);
  if (content::Details<std::pair<InfoBarDelegate*, bool> >(details)->first !=
      this)
    return;
  if (!accepted_) {
    content::BrowserContext::GetDefaultStoragePartition(browser_->profile())->
        GetDOMStorageContext()->StartScavengingUnusedSessionStorage();
    removed_notification_received_ = true;
  }
}

}  // namespace

namespace chrome {

void ShowSessionCrashedPrompt(Browser* browser) {
  // Assume that if the user is launching incognito they were previously
  // running incognito so that we have nothing to restore from.
  if (browser->profile()->IsOffTheRecord())
    return;

  // In ChromeBot tests, there might be a race. This line appears to get
  // called during shutdown and |tab| can be NULL.
  content::WebContents* tab =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!tab)
    return;

  // Don't show the info-bar if there are already info-bars showing.
  InfoBarService* infobar_service = InfoBarService::FromWebContents(tab);
  if (infobar_service->GetInfoBarCount() > 0)
    return;

  infobar_service->AddInfoBar(
      new SessionCrashedInfoBarDelegate(infobar_service, browser));
}

}  // namespace chrome
