// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_MEDIA_MEDIA_THROTTLE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_MEDIA_MEDIA_THROTTLE_INFOBAR_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class WebContents;
}

class MediaThrottleInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  typedef base::Callback<void(bool)> DecodeRequestGrantedCallback;
  ~MediaThrottleInfoBarDelegate() override;

  // Static method to create the object
  static void Create(
      content::WebContents* web_contents,
      const DecodeRequestGrantedCallback& callback);

 private:
  explicit MediaThrottleInfoBarDelegate(
      const DecodeRequestGrantedCallback& callback);

  // ConfirmInfoBarDelegate:
  MediaThrottleInfoBarDelegate* AsMediaThrottleInfoBarDelegate() override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  void InfoBarDismissed() override;

  DecodeRequestGrantedCallback decode_granted_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaThrottleInfoBarDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_MEDIA_MEDIA_THROTTLE_INFOBAR_DELEGATE_H_
