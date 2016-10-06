// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/content_settings/subresource_filter_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// static
void SubresourceFilterInfobarDelegate::Create(InfoBarService* infobar_service) {
  infobar_service->AddInfoBar(CreateSubresourceFilterInfoBar(
      base::WrapUnique(new SubresourceFilterInfobarDelegate())));
}

SubresourceFilterInfobarDelegate::SubresourceFilterInfobarDelegate()
    : ConfirmInfoBarDelegate() {}

SubresourceFilterInfobarDelegate::~SubresourceFilterInfobarDelegate() {}

base::string16 SubresourceFilterInfobarDelegate::GetExplanationText() const {
  return l10n_util::GetStringUTF16(
      IDS_FILTERED_DECEPTIVE_CONTENT_PROMPT_EXPLANATION);
}

infobars::InfoBarDelegate::InfoBarIdentifier
SubresourceFilterInfobarDelegate::GetIdentifier() const {
  return SUBRESOURCE_FILTER_INFOBAR_DELEGATE_ANDROID;
}

int SubresourceFilterInfobarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_SUBRESOURCE_FILTERING;
}

base::string16 SubresourceFilterInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_FILTERED_DECEPTIVE_CONTENT_PROMPT_TITLE);
}

int SubresourceFilterInfobarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

base::string16 SubresourceFilterInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      (button == BUTTON_OK) ? IDS_OK
                            : IDS_FILTERED_DECEPTIVE_CONTENT_PROMPT_RELOAD);
}

bool SubresourceFilterInfobarDelegate::Cancel() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  subresource_filter::ContentSubresourceFilterDriverFactory::FromWebContents(
      web_contents)
      ->OnReloadRequested();
  return true;
}
