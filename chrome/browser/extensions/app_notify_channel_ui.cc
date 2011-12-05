// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notify_channel_ui.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_wizard.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

class AppNotifyChannelUIImpl::InfoBar : public ConfirmInfoBarDelegate {
 public:
  InfoBar(AppNotifyChannelUIImpl* creator,
          InfoBarTabHelper* helper,
          const std::string& app_name);
  virtual ~InfoBar();

  // ConfirmInfoBarDelegate.
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;

 private:
  AppNotifyChannelUIImpl* creator_;
  std::string app_name_;

  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

AppNotifyChannelUIImpl::InfoBar::InfoBar(
    AppNotifyChannelUIImpl* creator,
    InfoBarTabHelper* helper,
    const std::string& app_name)
    : ConfirmInfoBarDelegate(helper), creator_(creator), app_name_(app_name) {
}

AppNotifyChannelUIImpl::InfoBar::~InfoBar() {}

string16 AppNotifyChannelUIImpl::InfoBar::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_APP_NOTIFICATION_NEED_SIGNIN,
                                    UTF8ToUTF16(app_name_));
}

string16 AppNotifyChannelUIImpl::InfoBar::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return l10n_util::GetStringUTF16(IDS_APP_NOTIFICATION_NEED_SIGNIN_ACCEPT);
  } else if (button == BUTTON_CANCEL) {
    return l10n_util::GetStringUTF16(IDS_APP_NOTIFICATION_NEED_SIGNIN_CANCEL);
  } else {
    NOTREACHED();
  }
  return string16();
}

bool AppNotifyChannelUIImpl::InfoBar::Accept() {
  creator_->OnInfoBarResult(true);
  return true;
}

bool AppNotifyChannelUIImpl::InfoBar::Cancel() {
  creator_->OnInfoBarResult(false);
  return true;
}

void AppNotifyChannelUIImpl::InfoBar::InfoBarDismissed() {
  Cancel();
}


AppNotifyChannelUIImpl::AppNotifyChannelUIImpl(Browser* browser,
                                               TabContentsWrapper* wrapper,
                                               const std::string& app_name)
    : browser_(browser), wrapper_(wrapper), app_name_(app_name),
      delegate_(NULL), observing_sync_(false), wizard_shown_to_user_(false) {
}

AppNotifyChannelUIImpl::~AppNotifyChannelUIImpl() {
  // We should have either not started observing sync, or already called
  // StopObservingSync by this point.
  CHECK(!observing_sync_);
}

void AppNotifyChannelUIImpl::PromptSyncSetup(
    AppNotifyChannelUI::Delegate* delegate) {
  CHECK(delegate_ == NULL);
  delegate_ = delegate;

  if (!browser_->profile()->HasProfileSyncService()) {
    delegate_->OnSyncSetupResult(false);
    return;
  }

  InfoBarTabHelper* helper = wrapper_->infobar_tab_helper();
  helper->AddInfoBar(new AppNotifyChannelUIImpl::InfoBar(
      this, helper, app_name_));
}

void AppNotifyChannelUIImpl::OnInfoBarResult(bool accepted) {
  if (accepted) {
    StartObservingSync();
    ProfileSyncService* service =
        browser_->profile()->GetOriginalProfile()->GetProfileSyncService();
    service->ShowLoginDialog();
  } else {
    delegate_->OnSyncSetupResult(false);
  }
}

void AppNotifyChannelUIImpl::OnStateChanged() {
  ProfileSyncService* sync_service =
      browser_->profile()->GetProfileSyncService();
  bool wizard_visible = sync_service->WizardIsVisible();
  // ProfileSyncService raises OnStateChanged many times. Even multiple
  // times before the wizard actually becomes visible for the first time.
  // So we have to wait for the wizard to become visible once and then we
  // wait for it to get dismissed.
  bool finished = wizard_shown_to_user_ && !wizard_visible;
  if (wizard_visible)
    wizard_shown_to_user_ = true;

  if (finished) {
    StopObservingSync();
    delegate_->OnSyncSetupResult(sync_service->HasSyncSetupCompleted());
  }
}

void AppNotifyChannelUIImpl::StartObservingSync() {
  CHECK(!observing_sync_);
  observing_sync_ = true;
  browser_->profile()->GetProfileSyncService()->AddObserver(this);
}

void AppNotifyChannelUIImpl::StopObservingSync() {
  CHECK(observing_sync_);
  observing_sync_ = false;
  browser_->profile()->GetProfileSyncService()->RemoveObserver(this);
}
