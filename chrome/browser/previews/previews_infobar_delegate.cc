// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_infobar_delegate.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_infobar_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

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

PreviewsInfoBarDelegate::~PreviewsInfoBarDelegate() {}

// static
void PreviewsInfoBarDelegate::Create(content::WebContents* web_contents,
                                     PreviewsInfoBarType infobar_type) {
  PreviewsInfoBarTabHelper* infobar_tab_helper =
      PreviewsInfoBarTabHelper::FromWebContents(web_contents);
  if (infobar_tab_helper->displayed_preview_infobar())
    return;

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);

  infobars::InfoBar* infobar =
      infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
          std::unique_ptr<ConfirmInfoBarDelegate>(
              new PreviewsInfoBarDelegate(web_contents, infobar_type))));

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
    PreviewsInfoBarType infobar_type)
    : ConfirmInfoBarDelegate(),
      infobar_type_(infobar_type) {}

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
  RecordPreviewsInfoBarAction(infobar_type_, INFOBAR_DISMISSED_BY_NAVIGATION);
  return InfoBarDelegate::ShouldExpire(details);
}

void PreviewsInfoBarDelegate::InfoBarDismissed() {
  RecordPreviewsInfoBarAction(infobar_type_, INFOBAR_DISMISSED_BY_USER);
}

base::string16 PreviewsInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16((infobar_type_ == OFFLINE)
                                       ? IDS_PREVIEWS_INFOBAR_FASTER_PAGE_TITLE
                                       : IDS_PREVIEWS_INFOBAR_SAVED_DATA_TITLE);
}

int PreviewsInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

base::string16 PreviewsInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_LINK);
}

bool PreviewsInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  RecordPreviewsInfoBarAction(infobar_type_, INFOBAR_LOAD_ORIGINAL_CLICKED);

  if (infobar_type_ == LITE_PAGE || infobar_type_ == LOFI) {
    auto* web_contents =
        InfoBarService::WebContentsFromInfoBar(infobar());

    if (infobar_type_ == LITE_PAGE)
      web_contents->GetController().ReloadDisableLoFi(true);
    else if (infobar_type_ == LOFI)
      web_contents->ReloadLoFiImages();

    auto* data_reduction_proxy_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            web_contents->GetBrowserContext());
    data_reduction_proxy_settings->IncrementLoFiUserRequestsForImages();
  }

  return true;
}
