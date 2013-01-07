// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/frame_navigate_params.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class ManagedModeWarningInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit ManagedModeWarningInfobarDelegate(InfoBarService* infobar_service);

 private:
  virtual ~ManagedModeWarningInfobarDelegate();

  // ConfirmInfoBarDelegate overrides:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // InfoBarDelegate override:
  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeWarningInfobarDelegate);
};

void GoBackToSafety(content::WebContents* web_contents) {
  // For now, just go back one page (the user didn't retreat from that page,
  // so it should be okay).
  content::NavigationController* controller =
      &web_contents->GetController();
  if (controller->CanGoBack()) {
    controller->GoBack();
    return;
  }

  // If we can't go back (because we opened a new tab), try to close the tab.
  // If this is the last tab, open a new window.
  if (BrowserList::size() == 1) {
    Browser* browser = *(BrowserList::begin());
    DCHECK(browser == chrome::FindBrowserWithWebContents(web_contents));
    if (browser->tab_count() == 1)
      chrome::NewEmptyWindow(browser->profile());
  }

  web_contents->GetDelegate()->CloseContents(web_contents);
}

ManagedModeWarningInfobarDelegate::ManagedModeWarningInfobarDelegate(
    InfoBarService* infobar_service)
    : ConfirmInfoBarDelegate(infobar_service) {}

ManagedModeWarningInfobarDelegate::~ManagedModeWarningInfobarDelegate() {}

string16 ManagedModeWarningInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MANAGED_MODE_WARNING_MESSAGE);
}

int ManagedModeWarningInfobarDelegate::GetButtons() const {
  return BUTTON_OK;
}

string16 ManagedModeWarningInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_MANAGED_MODE_GO_BACK_ACTION);
}

bool ManagedModeWarningInfobarDelegate::Accept() {
  GoBackToSafety(owner()->GetWebContents());

  return false;
}

bool ManagedModeWarningInfobarDelegate::Cancel() {
  NOTREACHED();
  return false;
}

bool ManagedModeWarningInfobarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  // ManagedModeNavigationObserver removes us below.
  return false;
}

void ManagedModeWarningInfobarDelegate::InfoBarDismissed() {
  ManagedModeNavigationObserver* observer =
      ManagedModeNavigationObserver::FromWebContents(owner()->GetWebContents());
  observer->WarnInfobarDismissed();
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagedModeNavigationObserver);

ManagedModeNavigationObserver::~ManagedModeNavigationObserver() {}

ManagedModeNavigationObserver::ManagedModeNavigationObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      url_filter_(ManagedMode::GetURLFilterForUIThread()),
      warn_infobar_delegate_(NULL) {}

void ManagedModeNavigationObserver::WarnInfobarDismissed() {
  DCHECK(warn_infobar_delegate_);
  warn_infobar_delegate_ = NULL;
}

void ManagedModeNavigationObserver::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  if (!is_main_frame)
    return;

  ManagedModeURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(url);

  if (behavior == ManagedModeURLFilter::WARN) {
    if (!warn_infobar_delegate_) {
      InfoBarService* infobar_service =
          InfoBarService::FromWebContents(web_contents());
      warn_infobar_delegate_ =
          new ManagedModeWarningInfobarDelegate(infobar_service);
      infobar_service->AddInfoBar(warn_infobar_delegate_);
    }
  } else {
    if (warn_infobar_delegate_) {
      InfoBarService* infobar_service =
          InfoBarService::FromWebContents(web_contents());
      infobar_service->RemoveInfoBar(warn_infobar_delegate_);
      warn_infobar_delegate_= NULL;
    }
  }

}
