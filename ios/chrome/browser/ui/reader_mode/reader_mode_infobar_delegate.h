// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_INFOBAR_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

// This is the actual infobar displayed to prompt the user to switch to reader
// mode.
class ReaderModeInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit ReaderModeInfoBarDelegate(const base::Closure& callback);
  ~ReaderModeInfoBarDelegate() override;

  // InfoBarDelegate methods.
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate methods.
  base::string16 GetMessageText() const override;
  bool Accept() override;

 private:
  std::string html_;
  GURL url_;
  base::Closure callback_;
};

#endif  // IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_INFOBAR_DELEGATE_H_
