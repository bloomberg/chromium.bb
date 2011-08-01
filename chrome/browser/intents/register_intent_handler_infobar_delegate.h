// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_REGISTER_INTENT_HANDLER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_INTENTS_REGISTER_INTENT_HANDLER_INFOBAR_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"

class TabContents;

// The InfoBar used to request permission for a site to be registered as an
// Intent handler.
class RegisterIntentHandlerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // TODO(jhawkins): Pass in WebIntentData.
  explicit RegisterIntentHandlerInfoBarDelegate(TabContents* tab_contents);

  // ConfirmInfoBarDelegate implementation.
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

 private:
  // The TabContents that contains this InfoBar. Weak pointer.
  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(RegisterIntentHandlerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_INTENTS_REGISTER_INTENT_HANDLER_INFOBAR_DELEGATE_H_
