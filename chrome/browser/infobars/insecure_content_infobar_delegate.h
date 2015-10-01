// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INSECURE_CONTENT_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_INFOBARS_INSECURE_CONTENT_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;

// Base class for delegates that show warnings on HTTPS pages which try to
// display or run insecure content.
class InsecureContentInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an insecure content infobar and delegate and adds the infobar to
  // |infobar_service|, replacing any existing insecure content infobar.
  static void Create(InfoBarService* infobar_service);

 private:
  enum HistogramEvents {
    DISPLAY_INFOBAR_SHOWN = 0,  // Infobar was displayed.
    DISPLAY_USER_OVERRIDE,      // User clicked allow anyway button.
    DISPLAY_USER_DID_NOT_LOAD,  // User clicked the don't load button.
    DISPLAY_INFOBAR_DISMISSED,  // User clicked close button.
    NUM_EVENTS
  };

  InsecureContentInfoBarDelegate();
  ~InsecureContentInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  void InfoBarDismissed() override;
  InsecureContentInfoBarDelegate* AsInsecureContentInfoBarDelegate() override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;

  DISALLOW_COPY_AND_ASSIGN(InsecureContentInfoBarDelegate);
};

#endif  // CHROME_BROWSER_INFOBARS_INSECURE_CONTENT_INFOBAR_DELEGATE_H_

