// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTERNAL_PROTOCOL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTERNAL_PROTOCOL_DIALOG_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "chrome/browser/ui/protocol_dialog_delegate.h"
#include "googleurl/src/gurl.h"

// Provides text for the external protocol handler dialog and handles whether
// or not to launch the application for the given protocol.
class ExternalProtocolDialogDelegate : public ProtocolDialogDelegate {
 public:
  explicit ExternalProtocolDialogDelegate(const GURL& url);
  virtual ~ExternalProtocolDialogDelegate();

  virtual void DoAccept(const GURL& url, bool dont_block) const OVERRIDE;
  virtual void DoCancel(const GURL& url, bool dont_block) const OVERRIDE;

  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetCheckboxText() const OVERRIDE;
  virtual string16 GetTitleText() const OVERRIDE;
};

#endif  // CHROME_BROWSER_UI_EXTERNAL_PROTOCOL_DIALOG_DELEGATE_H_
