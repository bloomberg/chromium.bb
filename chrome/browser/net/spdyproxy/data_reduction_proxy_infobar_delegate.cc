// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_infobar_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// static
void DataReductionProxyInfoBarDelegate::Create(
    content::WebContents* web_contents, const std::string& link_url) {
  InfoBarService::FromWebContents(web_contents)->AddInfoBar(
      DataReductionProxyInfoBarDelegate::CreateInfoBar(
          scoped_ptr<DataReductionProxyInfoBarDelegate>(
              new DataReductionProxyInfoBarDelegate(link_url))));
}

#if !defined(OS_ANDROID)
// This infobar currently only supports Android.

// static
scoped_ptr<infobars::InfoBar> DataReductionProxyInfoBarDelegate::CreateInfoBar(
    scoped_ptr<DataReductionProxyInfoBarDelegate> delegate) {
  return ConfirmInfoBarDelegate::CreateInfoBar(
      delegate.PassAs<ConfirmInfoBarDelegate>());
}
#endif

DataReductionProxyInfoBarDelegate::~DataReductionProxyInfoBarDelegate() {
}

DataReductionProxyInfoBarDelegate::DataReductionProxyInfoBarDelegate(
    const std::string& link_url)
    : ConfirmInfoBarDelegate(),
      link_url_(link_url) {
}

bool DataReductionProxyInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

base::string16 DataReductionProxyInfoBarDelegate::GetMessageText() const {
  return base::string16();
}

int DataReductionProxyInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

bool DataReductionProxyInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(
          GURL(link_url_),
          content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          ui::PAGE_TRANSITION_LINK, false));
  return true;
}
