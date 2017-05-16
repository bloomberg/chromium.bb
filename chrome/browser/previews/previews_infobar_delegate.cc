// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_infobar_delegate.h"

#include "base/metrics/histogram.h"
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

void RecordPreviewsInfoBarAction(
    previews::PreviewsType previews_type,
    PreviewsInfoBarDelegate::PreviewsInfoBarAction action) {
  int32_t max_limit =
      static_cast<int32_t>(PreviewsInfoBarDelegate::INFOBAR_INDEX_BOUNDARY);
  base::LinearHistogram::FactoryGet(
      base::StringPrintf("Previews.InfoBarAction.%s",
                         GetStringNameForType(previews_type).c_str()),
      1, max_limit, max_limit + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(static_cast<int32_t>(action));
}

// Sends opt out information to the pingback service based on a key value in the
// infobar tab helper. Sending this information in the case of a navigation that
// should not send a pingback (or is not a server preview) will not alter the
// pingback.
void ReportPingbackInformation(content::WebContents* web_contents) {
  auto* data_reduction_proxy_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  PreviewsInfoBarTabHelper* infobar_tab_helper =
      PreviewsInfoBarTabHelper::FromWebContents(web_contents);
  if (infobar_tab_helper &&
      infobar_tab_helper->committed_data_saver_navigation_id()) {
    data_reduction_proxy_settings->data_reduction_proxy_service()
        ->pingback_client()
        ->AddOptOut(
            infobar_tab_helper->committed_data_saver_navigation_id().value());
  }
}

// Increments the prefs-based opt out information for data reduction proxy when
// the user is not already transitioned to the PreviewsBlackList.
void IncrementDataReductionProxyPrefs(content::WebContents* web_contents) {
  auto* data_reduction_proxy_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  data_reduction_proxy_settings->IncrementLoFiUserRequestsForImages();
}

// Reloads the content of the page without previews.
void ReloadWithoutPreviews(previews::PreviewsType previews_type,
                           content::WebContents* web_contents) {
  switch (previews_type) {
    case previews::PreviewsType::LITE_PAGE:
    case previews::PreviewsType::OFFLINE:
      // Prevent LoFi and lite page modes from showing after reload.
      // TODO(ryansturm): rename DISABLE_LOFI_MODE to DISABLE_PREVIEWS.
      // crbug.com/707272
      web_contents->GetController().Reload(
          content::ReloadType::DISABLE_LOFI_MODE, true);
      break;
    case previews::PreviewsType::LOFI:
      web_contents->ReloadLoFiImages();
      break;
    case previews::PreviewsType::NONE:
    case previews::PreviewsType::LAST:
      break;
  }
}

}  // namespace

PreviewsInfoBarDelegate::~PreviewsInfoBarDelegate() {
  if (!on_dismiss_callback_.is_null())
    on_dismiss_callback_.Run(false);

  RecordPreviewsInfoBarAction(previews_type_, infobar_dismissed_action_);
}

// static
void PreviewsInfoBarDelegate::Create(
    content::WebContents* web_contents,
    previews::PreviewsType previews_type,
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
      web_contents, previews_type, is_data_saver_user, on_dismiss_callback));

#if defined(OS_ANDROID)
  std::unique_ptr<infobars::InfoBar> infobar_ptr(
      PreviewsInfoBar::CreateInfoBar(infobar_service, std::move(delegate)));
#else
  std::unique_ptr<infobars::InfoBar> infobar_ptr(
      infobar_service->CreateConfirmInfoBar(std::move(delegate)));
#endif

  infobars::InfoBar* infobar =
      infobar_service->AddInfoBar(std::move(infobar_ptr));

  if (infobar && (previews_type == previews::PreviewsType::LITE_PAGE ||
                  previews_type == previews::PreviewsType::LOFI)) {
    auto* data_reduction_proxy_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            web_contents->GetBrowserContext());
    data_reduction_proxy_settings->IncrementLoFiUIShown();
  }

  RecordPreviewsInfoBarAction(previews_type, INFOBAR_SHOWN);
  infobar_tab_helper->set_displayed_preview_infobar(true);
}

PreviewsInfoBarDelegate::PreviewsInfoBarDelegate(
    content::WebContents* web_contents,
    previews::PreviewsType previews_type,
    bool is_data_saver_user,
    const OnDismissPreviewsInfobarCallback& on_dismiss_callback)
    : ConfirmInfoBarDelegate(),
      previews_type_(previews_type),
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

  ReportPingbackInformation(web_contents);

  if ((previews_type_ == previews::PreviewsType::LITE_PAGE ||
       previews_type_ == previews::PreviewsType::LOFI) &&
      !data_reduction_proxy::params::IsBlackListEnabledForServerPreviews()) {
    IncrementDataReductionProxyPrefs(web_contents);
  }

  ReloadWithoutPreviews(previews_type_, web_contents);

  return true;
}

base::string16 PreviewsInfoBarDelegate::GetTimestampText() const {
  return base::string16();
}
