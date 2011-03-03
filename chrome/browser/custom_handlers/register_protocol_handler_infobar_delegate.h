// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_INFOBAR_DELEGATE_H_
#pragma once

#include "base/string16.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"

class SkBitmap;
class ProtocolHandler;
class ProtocolHandlerRegistry;
class TabContents;

// An InfoBar delegate that enables the user to allow or deny storing credit
// card information gathered from a form submission.
class RegisterProtocolHandlerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  RegisterProtocolHandlerInfoBarDelegate(TabContents* tab_contents,
                                         ProtocolHandlerRegistry* registry,
                                         ProtocolHandler* handler);

  // ConfirmInfoBarDelegate:
  virtual bool ShouldExpire(const NavigationController::LoadCommittedDetails&
      details) const OVERRIDE;
  virtual void InfoBarClosed() OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

 private:
  TabContents* tab_contents_;
  ProtocolHandlerRegistry* registry_;
  ProtocolHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(RegisterProtocolHandlerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_INFOBAR_DELEGATE_H_
