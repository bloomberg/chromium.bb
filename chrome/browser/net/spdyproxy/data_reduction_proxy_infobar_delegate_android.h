// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_INFOBAR_DELEGATE_ANDROID_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"

namespace content {
class WebContents;
}

namespace infobars {
class InfoBarManager;
}

// Configures an InfoBar with no buttons that stays visible until it is
// explicitly dismissed. This InfoBar is suitable for displaying a message
// and a link, and is used only by an Android field trial.
class DataReductionProxyInfoBarDelegateAndroid : public ConfirmInfoBarDelegate {
 public:
  // Creates the InfoBar and adds it to the provided |web_contents|.
  static void Create(content::WebContents* web_contents,
                     const std::string& link_url);

  ~DataReductionProxyInfoBarDelegateAndroid() override;

 private:
  explicit DataReductionProxyInfoBarDelegateAndroid(
      const std::string& link_url);

  // Returns a Data Reduction Proxy infobar that owns |delegate|.
  static scoped_ptr<infobars::InfoBar> CreateInfoBar(
      infobars::InfoBarManager* infobar_manager,
      scoped_ptr<DataReductionProxyInfoBarDelegateAndroid> delegate);

  // ConfirmInfoBarDelegate
  bool ShouldExpire(const NavigationDetails& details) const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  GURL GetLinkURL() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

  std::string link_url_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_INFOBAR_DELEGATE_ANDROID_H_
