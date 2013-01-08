// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_INFOBAR_DELEGATE_H_

#include "base/string16.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/common/custom_handlers/protocol_handler.h"

class InfoBarService;
class ProtocolHandlerRegistry;

// An InfoBar delegate that enables the user to allow or deny storing credit
// card information gathered from a form submission.
class RegisterProtocolHandlerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a new RPH delegate.  Searches |infobar_service| for an existing
  // delegate for the same |handler|; replaces it with the new delegate if
  // found, otherwise adds the new infobar to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     ProtocolHandlerRegistry* registry,
                     const ProtocolHandler& handler);

  // ConfirmInfoBarDelegate:
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool NeedElevation(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  virtual RegisterProtocolHandlerInfoBarDelegate*
      AsRegisterProtocolHandlerInfoBarDelegate() OVERRIDE;

  virtual InfoBarAutomationType GetInfoBarAutomationType() const OVERRIDE;

 private:
  RegisterProtocolHandlerInfoBarDelegate(InfoBarService* infobar_service,
                                         ProtocolHandlerRegistry* registry,
                                         const ProtocolHandler& handler);

  // Returns a user-friendly name for the protocol of this protocol handler.
  string16 GetProtocolName(const ProtocolHandler& handler) const;
  ProtocolHandlerRegistry* registry_;
  ProtocolHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(RegisterProtocolHandlerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_INFOBAR_DELEGATE_H_
