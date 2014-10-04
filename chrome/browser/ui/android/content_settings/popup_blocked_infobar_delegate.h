// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_POPUP_BLOCKED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_POPUP_BLOCKED_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

class HostContentSettingsMap;

class PopupBlockedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a popup blocked infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(content::WebContents* web_contents, int num_popups);

  virtual ~PopupBlockedInfoBarDelegate();

 private:
  PopupBlockedInfoBarDelegate(int num_popups,
                              const GURL& url,
                              HostContentSettingsMap* map);

  // ConfirmInfoBarDelegate:
  virtual int GetIconID() const override;
  virtual PopupBlockedInfoBarDelegate* AsPopupBlockedInfoBarDelegate() override;
  virtual base::string16 GetMessageText() const override;
  virtual int GetButtons() const override;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const override;
  virtual bool Accept() override;

  int num_popups_;
  GURL url_;
  HostContentSettingsMap* map_;
  bool can_show_popups_;

  DISALLOW_COPY_AND_ASSIGN(PopupBlockedInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_POPUP_BLOCKED_INFOBAR_DELEGATE_H_
