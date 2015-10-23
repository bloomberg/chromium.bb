// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_infobar_delegate_android.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// static
void DataReductionProxyInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents,
    const std::string& link_url) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->AddInfoBar(
      DataReductionProxyInfoBarDelegateAndroid::CreateInfoBar(
          infobar_service,
          scoped_ptr<DataReductionProxyInfoBarDelegateAndroid>(
              new DataReductionProxyInfoBarDelegateAndroid(link_url))));
}

DataReductionProxyInfoBarDelegateAndroid::
    ~DataReductionProxyInfoBarDelegateAndroid() {}

DataReductionProxyInfoBarDelegateAndroid::
    DataReductionProxyInfoBarDelegateAndroid(const std::string& link_url)
    : ConfirmInfoBarDelegate(), link_url_(link_url) {}

bool DataReductionProxyInfoBarDelegateAndroid::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

base::string16 DataReductionProxyInfoBarDelegateAndroid::GetMessageText()
    const {
  return base::string16();
}

int DataReductionProxyInfoBarDelegateAndroid::GetButtons() const {
  return BUTTON_NONE;
}

GURL DataReductionProxyInfoBarDelegateAndroid::GetLinkURL() const {
  return GURL(link_url_);
}

bool DataReductionProxyInfoBarDelegateAndroid::LinkClicked(
    WindowOpenDisposition disposition) {
  ConfirmInfoBarDelegate::LinkClicked(disposition);
  return true;
}
