// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_INFOBAR_DELEGATE_H_

#include "base/strings/string16.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;
class ProtocolHandlerRegistry;

// An InfoBar delegate that enables the user to allow or deny storing credit
// card information gathered from a form submission.
class RegisterProtocolHandlerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a new register protocol handler infobar and delegate.  Searches
  // |infobar_service| for an existing infobar for the same |handler|; replaces
  // it with the new infobar if found, otherwise adds the new infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     ProtocolHandlerRegistry* registry,
                     const ProtocolHandler& handler);

 private:
  RegisterProtocolHandlerInfoBarDelegate(ProtocolHandlerRegistry* registry,
                                         const ProtocolHandler& handler);
  ~RegisterProtocolHandlerInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  Type GetInfoBarType() const override;
  InfoBarAutomationType GetInfoBarAutomationType() const override;
  RegisterProtocolHandlerInfoBarDelegate*
      AsRegisterProtocolHandlerInfoBarDelegate() override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool OKButtonTriggersUACPrompt() const override;
  bool Accept() override;
  bool Cancel() override;
  base::string16 GetLinkText() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

  // Returns a user-friendly name for the protocol of this protocol handler.
  base::string16 GetProtocolName(const ProtocolHandler& handler) const;

  ProtocolHandlerRegistry* registry_;
  ProtocolHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(RegisterProtocolHandlerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_INFOBAR_DELEGATE_H_
