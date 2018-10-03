// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_infobar_delegate.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void RecordInfoBarAction(
    PreviewsLitePageInfoBarDelegate::PreviewsLitePageInfoBarAction action) {
  UMA_HISTOGRAM_ENUMERATION("Previews.LitePageNotificationInfoBar", action);
}

}  // namespace

// static
void PreviewsLitePageInfoBarDelegate::Create(
    content::WebContents* web_contents) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  if (!infobar_service)
    return;

  std::unique_ptr<PreviewsLitePageInfoBarDelegate> delegate(
      new PreviewsLitePageInfoBarDelegate());

  std::unique_ptr<infobars::InfoBar> infobar_ptr(
      infobar_service->CreateConfirmInfoBar(std::move(delegate)));

  RecordInfoBarAction(kInfoBarShown);
  infobar_service->AddInfoBar(std::move(infobar_ptr));
}

PreviewsLitePageInfoBarDelegate::PreviewsLitePageInfoBarDelegate() {}
PreviewsLitePageInfoBarDelegate::~PreviewsLitePageInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
PreviewsLitePageInfoBarDelegate::GetIdentifier() const {
  return LITE_PAGE_PREVIEWS_INFOBAR;
}

// TODO(robertogden): Add link on Android.

int PreviewsLitePageInfoBarDelegate::GetIconId() const {
  // TODO(robertogden): Add an Android icon.
  return kNoIconID;
}

void PreviewsLitePageInfoBarDelegate::InfoBarDismissed() {
  RecordInfoBarAction(kInfoBarDismissed);
}

base::string16 PreviewsLitePageInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_LITE_PAGE_PREVIEWS_MESSAGE);
}

int PreviewsLitePageInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}
