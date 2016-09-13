// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_infobar_delegate.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

PreviewsInfoBarDelegate::~PreviewsInfoBarDelegate() {}

// static
void PreviewsInfoBarDelegate::Create(content::WebContents* web_contents,
                                     PreviewsInfoBarType infobar_type) {
  // TODO(megjablon): Check that the infobar was not already shown.

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
  // TODO(megjablon): Record UMA data.
  return InfoBarDelegate::ShouldExpire(details);
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
  // TODO(megjablon): Record UMA data.
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
