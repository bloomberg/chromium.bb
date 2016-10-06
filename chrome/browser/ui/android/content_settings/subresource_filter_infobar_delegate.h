// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_SUBRESOURCE_FILTER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_SUBRESOURCE_FILTER_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;

// This infobar appears when the user proceeds through Safe Browsing warning
// interstitials to a site with deceptive embedded content. It tells the user
// some content has been removed and provides a button to reload the page with
// the content unblocked.
class SubresourceFilterInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a subresource filter infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service);

  ~SubresourceFilterInfobarDelegate() override;

  base::string16 GetExplanationText() const;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Cancel() override;

 private:
  SubresourceFilterInfobarDelegate();

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterInfobarDelegate);
};

std::unique_ptr<infobars::InfoBar> CreateSubresourceFilterInfoBar(
    std::unique_ptr<SubresourceFilterInfobarDelegate> delegate);

#endif  // CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_SUBRESOURCE_FILTER_INFOBAR_DELEGATE_H_
