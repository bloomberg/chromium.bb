// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_infobar_delegate.h"

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_infobar_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/infobars/previews_infobar.h"
#endif

namespace {

// Key of the UMA Previews.InfoBarAction.LoFi histogram.
const char kUMAPreviewsInfoBarActionLoFi[] = "Previews.InfoBarAction.LoFi";

// Key of the UMA Previews.InfoBarAction.Offline histogram.
const char kUMAPreviewsInfoBarActionOffline[] =
    "Previews.InfoBarAction.Offline";

// Key of the UMA Previews.InfoBarAction.LitePage histogram.
const char kUMAPreviewsInfoBarActionLitePage[] =
    "Previews.InfoBarAction.LitePage";

void RecordPreviewsInfoBarAction(
    PreviewsInfoBarDelegate::PreviewsInfoBarType infobar_type,
    PreviewsInfoBarDelegate::PreviewsInfoBarAction action) {
  if (infobar_type == PreviewsInfoBarDelegate::LOFI) {
    UMA_HISTOGRAM_ENUMERATION(kUMAPreviewsInfoBarActionLoFi, action,
                              PreviewsInfoBarDelegate::INFOBAR_INDEX_BOUNDARY);
  } else if (infobar_type == PreviewsInfoBarDelegate::LITE_PAGE) {
    UMA_HISTOGRAM_ENUMERATION(kUMAPreviewsInfoBarActionLitePage, action,
                              PreviewsInfoBarDelegate::INFOBAR_INDEX_BOUNDARY);
  } else if (infobar_type == PreviewsInfoBarDelegate::OFFLINE) {
    UMA_HISTOGRAM_ENUMERATION(kUMAPreviewsInfoBarActionOffline, action,
                              PreviewsInfoBarDelegate::INFOBAR_INDEX_BOUNDARY);
  }
}

}  // namespace

PreviewsInfoBarDelegate::~PreviewsInfoBarDelegate() {
  if (!on_dismiss_callback_.is_null())
    on_dismiss_callback_.Run(false);

  RecordPreviewsInfoBarAction(infobar_type_, infobar_dismissed_action_);
}

// static
void PreviewsInfoBarDelegate::Create(
    content::WebContents* web_contents,
    PreviewsInfoBarType infobar_type,
    bool is_data_saver_user,
    const OnDismissPreviewsInfobarCallback& on_dismiss_callback) {
  PreviewsInfoBarTabHelper* infobar_tab_helper =
      PreviewsInfoBarTabHelper::FromWebContents(web_contents);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);

  // The WebContents may not have TabHelpers set. If TabHelpers are not set,
  // don't show Previews infobars.
  if (!infobar_tab_helper || !infobar_service)
    return;
  if (infobar_tab_helper->displayed_preview_infobar())
    return;

  std::unique_ptr<PreviewsInfoBarDelegate> delegate(new PreviewsInfoBarDelegate(
      web_contents, infobar_type, is_data_saver_user, on_dismiss_callback));

#if defined(OS_ANDROID)
  std::unique_ptr<infobars::InfoBar> infobar_ptr(
      PreviewsInfoBar::CreateInfoBar(infobar_service, std::move(delegate)));
#else
  std::unique_ptr<infobars::InfoBar> infobar_ptr(
      infobar_service->CreateConfirmInfoBar(std::move(delegate)));
#endif

  infobars::InfoBar* infobar =
      infobar_service->AddInfoBar(std::move(infobar_ptr));

  if (infobar && (infobar_type == LITE_PAGE || infobar_type == LOFI)) {
    auto* data_reduction_proxy_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            web_contents->GetBrowserContext());
    data_reduction_proxy_settings->IncrementLoFiUIShown();
  }

  RecordPreviewsInfoBarAction(infobar_type, INFOBAR_SHOWN);
  infobar_tab_helper->set_displayed_preview_infobar(true);
}

PreviewsInfoBarDelegate::PreviewsInfoBarDelegate(
    content::WebContents* web_contents,
    PreviewsInfoBarType infobar_type,
    bool is_data_saver_user,
    const OnDismissPreviewsInfobarCallback& on_dismiss_callback)
    : ConfirmInfoBarDelegate(),
      infobar_type_(infobar_type),
      infobar_dismissed_action_(INFOBAR_DISMISSED_BY_TAB_CLOSURE),
      message_text_(l10n_util::GetStringUTF16(
          is_data_saver_user ? IDS_PREVIEWS_INFOBAR_SAVED_DATA_TITLE
                             : IDS_PREVIEWS_INFOBAR_FASTER_PAGE_TITLE)),
      on_dismiss_callback_(on_dismiss_callback) {}

infobars::InfoBarDelegate::InfoBarIdentifier
PreviewsInfoBarDelegate::GetIdentifier() const {
  return DATA_REDUCTION_PROXY_PREVIEW_INFOBAR_DELEGATE;
}

int PreviewsInfoBarDelegate::GetIconId() const {
#if defined(OS_ANDROID)
  return IDR_ANDROID_INFOBAR_PREVIEWS;
#else
  return kNoIconID;
#endif
}

bool PreviewsInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  infobar_dismissed_action_ = details.is_reload
                                  ? INFOBAR_DISMISSED_BY_RELOAD
                                  : INFOBAR_DISMISSED_BY_NAVIGATION;
  return InfoBarDelegate::ShouldExpire(details);
}

void PreviewsInfoBarDelegate::InfoBarDismissed() {
  infobar_dismissed_action_ = INFOBAR_DISMISSED_BY_USER;
}

base::string16 PreviewsInfoBarDelegate::GetMessageText() const {
  return message_text_;
}

int PreviewsInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

base::string16 PreviewsInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_LINK);
}

bool PreviewsInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  infobar_dismissed_action_ = INFOBAR_LOAD_ORIGINAL_CLICKED;
  if (!on_dismiss_callback_.is_null())
    on_dismiss_callback_.Run(true);
  on_dismiss_callback_.Reset();

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  if (infobar_type_ == LITE_PAGE || infobar_type_ == LOFI) {
    auto* data_reduction_proxy_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            web_contents->GetBrowserContext());
    if (!data_reduction_proxy::params::IsBlackListEnabledForServerPreviews())
      data_reduction_proxy_settings->IncrementLoFiUserRequestsForImages();
    PreviewsInfoBarTabHelper* infobar_tab_helper =
        PreviewsInfoBarTabHelper::FromWebContents(web_contents);
    if (infobar_tab_helper &&
        infobar_tab_helper->committed_data_saver_navigation_id()) {
      data_reduction_proxy_settings->data_reduction_proxy_service()
          ->pingback_client()
          ->AddOptOut(
              infobar_tab_helper->committed_data_saver_navigation_id().value());
    }

    if (infobar_type_ == LITE_PAGE)
      web_contents->GetController().Reload(
          content::ReloadType::DISABLE_LOFI_MODE, true);
    else if (infobar_type_ == LOFI)
      web_contents->ReloadLoFiImages();
  } else if (infobar_type_ == OFFLINE) {
    // Prevent LoFi and lite page modes from showing after reload.
    // TODO(ryansturm): rename DISABLE_LOFI_MODE to DISABLE_PREVIEWS.
    //  crbug.com/707272
    web_contents->GetController().Reload(content::ReloadType::DISABLE_LOFI_MODE,
                                         true);
  }

  return true;
}

base::string16 PreviewsInfoBarDelegate::GetTimestampText() const {
  return base::string16();
}
