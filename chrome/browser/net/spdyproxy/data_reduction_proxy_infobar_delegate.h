// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"

namespace content {
class WebContents;
}

// Configures an InfoBar with no buttons that stays visible until it is
// explicitly dismissed. This InfoBar is suitable for displaying a message
// and a link, and is used only by an Android field trial.
class DataReductionProxyInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates the InfoBar and adds it to the provided |web_contents|.
  static void Create(content::WebContents* web_contents,
                     const std::string& link_url);

  virtual ~DataReductionProxyInfoBarDelegate();

 private:
  explicit DataReductionProxyInfoBarDelegate(const std::string& link_url);

  // Returns a Data Reduction Proxy infobar that owns |delegate|.
  static scoped_ptr<infobars::InfoBar> CreateInfoBar(
      scoped_ptr<DataReductionProxyInfoBarDelegate> delegate);

  // ConfirmInfoBarDelegate
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual bool ShouldExpire(const NavigationDetails& details) const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  std::string link_url_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyInfoBarDelegate);
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_INFOBAR_DELEGATE_H_
