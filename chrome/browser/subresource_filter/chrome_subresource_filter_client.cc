// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/content_settings/subresource_filter_infobar_delegate.h"

ChromeSubresourceFilterClient::ChromeSubresourceFilterClient(
    content::WebContents* web_contents)
    : web_contents_(web_contents), shown_for_navigation_(false) {
  DCHECK(web_contents);
}

ChromeSubresourceFilterClient::~ChromeSubresourceFilterClient() {}

void ChromeSubresourceFilterClient::ToggleNotificationVisibility(
    bool visibility) {
  if (shown_for_navigation_ && visibility)
    return;
  shown_for_navigation_ = visibility;
  UMA_HISTOGRAM_BOOLEAN("SubresourceFilter.Prompt.NumVisibility", visibility);
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);
  content_settings->SetSubresourceBlocked(visibility);
#if defined(OS_ANDROID)
  if (visibility) {
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents_);

    SubresourceFilterInfobarDelegate::Create(infobar_service);
    content_settings->SetSubresourceBlockageIndicated();
  }
#endif
}
