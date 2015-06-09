// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_responder.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"

InfoBarResponder::InfoBarResponder(InfoBarService* infobar_service,
                                   bool should_accept)
    : infobar_service_(infobar_service), should_accept_(should_accept) {
  infobar_service_->AddObserver(this);
}

InfoBarResponder::~InfoBarResponder() {
  // This is safe even if we were already removed as an observer in
  // OnInfoBarAdded().
  infobar_service_->RemoveObserver(this);
}

void InfoBarResponder::OnInfoBarAdded(infobars::InfoBar* infobar) {
  infobar_service_->RemoveObserver(this);
  ConfirmInfoBarDelegate* delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  DCHECK(delegate);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&InfoBarResponder::Respond, base::Unretained(this), delegate));
}

void InfoBarResponder::Respond(ConfirmInfoBarDelegate* delegate) {
  if (should_accept_)
    delegate->Accept();
  else
    delegate->Cancel();
}
