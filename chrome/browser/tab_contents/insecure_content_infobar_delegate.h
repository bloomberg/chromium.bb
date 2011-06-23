// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_INSECURE_CONTENT_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_TAB_CONTENTS_INSECURE_CONTENT_INFOBAR_DELEGATE_H_
#pragma once

#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"

class TabContentsWrapper;

// Base class for delegates that show warnings on HTTPS pages which try to
// display or run insecure content.
class InsecureContentInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  enum InfoBarType {
    DISPLAY,  // Shown when "inactive" content (e.g. images) has been blocked.
    RUN,      // Shown when "active" content (e.g. script) has been blocked.
  };

  InsecureContentInfoBarDelegate(TabContentsWrapper* tab_contents,
                                 InfoBarType type);
  virtual ~InsecureContentInfoBarDelegate();

  InfoBarType type() const { return type_; }

 private:
  enum HistogramEvents {
    DISPLAY_INFOBAR_SHOWN = 0,  // Infobar was displayed.
    DISPLAY_USER_OVERRIDE,      // User clicked allowed anyway button.
    DISPLAY_INFOBAR_DISMISSED,  // User clicked close button.
    RUN_INFOBAR_SHOWN,
    RUN_USER_OVERRIDE,
    RUN_INFOBAR_DISMISSED,
    NUM_EVENTS
  };

  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual InsecureContentInfoBarDelegate*
      AsInsecureContentInfoBarDelegate() OVERRIDE;
  virtual string16 GetMessageText() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual string16 GetLinkText() OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  TabContentsWrapper* tab_contents_;
  InfoBarType type_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(InsecureContentInfoBarDelegate);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_INSECURE_CONTENT_INFOBAR_DELEGATE_H_

