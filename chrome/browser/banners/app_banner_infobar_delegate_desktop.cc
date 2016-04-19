// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_infobar_delegate_desktop.h"

#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_data_fetcher_desktop.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icons_public.h"

namespace banners {

AppBannerInfoBarDelegateDesktop::AppBannerInfoBarDelegateDesktop(
    scoped_refptr<AppBannerDataFetcherDesktop> fetcher,
    const content::Manifest& web_manifest,
    extensions::BookmarkAppHelper* bookmark_app_helper,
    int event_request_id)
    : ConfirmInfoBarDelegate(),
      fetcher_(fetcher),
      web_manifest_(web_manifest),
      bookmark_app_helper_(bookmark_app_helper),
      event_request_id_(event_request_id),
      has_user_interaction_(false) {
}

AppBannerInfoBarDelegateDesktop::~AppBannerInfoBarDelegateDesktop() {
  if (!has_user_interaction_)
    TrackUserResponse(USER_RESPONSE_WEB_APP_IGNORED);
}

// static
infobars::InfoBar* AppBannerInfoBarDelegateDesktop::Create(
    scoped_refptr<AppBannerDataFetcherDesktop> fetcher,
    content::WebContents* web_contents,
    const content::Manifest& web_manifest,
    extensions::BookmarkAppHelper* bookmark_app_helper,
    int event_request_id) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new AppBannerInfoBarDelegateDesktop(
              fetcher, web_manifest, bookmark_app_helper, event_request_id))));
}

infobars::InfoBarDelegate::Type
AppBannerInfoBarDelegateDesktop::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

int AppBannerInfoBarDelegateDesktop::GetIconId() const {
  return IDR_INFOBAR_APP_BANNER;
}

gfx::VectorIconId AppBannerInfoBarDelegateDesktop::GetVectorIconId() const {
#if defined(OS_MACOSX)
  return gfx::VectorIconId::VECTOR_ICON_NONE;
#else
  return gfx::VectorIconId::APPS;
#endif
}

base::string16 AppBannerInfoBarDelegateDesktop::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_ADD_TO_SHELF_INFOBAR_TITLE);
}

int AppBannerInfoBarDelegateDesktop::GetButtons() const {
  return BUTTON_OK;
}

base::string16 AppBannerInfoBarDelegateDesktop::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_ADD_TO_SHELF_INFOBAR_ADD_BUTTON);
}

bool AppBannerInfoBarDelegateDesktop::Accept() {
  TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
  has_user_interaction_ = true;

  bookmark_app_helper_->CreateFromAppBanner(
      base::Bind(&AppBannerDataFetcherDesktop::FinishCreateBookmarkApp,
                 fetcher_),
      web_manifest_);
  return true;
}

infobars::InfoBarDelegate::InfoBarIdentifier
AppBannerInfoBarDelegateDesktop::GetIdentifier() const {
  return APP_BANNER_INFOBAR_DELEGATE_DESKTOP;
}

void AppBannerInfoBarDelegateDesktop::InfoBarDismissed() {
  TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
  has_user_interaction_ = true;

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  if (web_contents) {
    fetcher_.get()->Cancel();

    web_contents->GetMainFrame()->Send(
        new ChromeViewMsg_AppBannerDismissed(
            web_contents->GetMainFrame()->GetRoutingID(),
            event_request_id_));

    AppBannerSettingsHelper::RecordBannerDismissEvent(
        web_contents, web_manifest_.start_url.spec(),
        AppBannerSettingsHelper::WEB);
  }
}

}  // namespace banners
