// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROTOCOL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_PROTOCOL_DIALOG_DELEGATE_H_

#include "base/basictypes.h"
#include "url/gurl.h"

// Interface implemented by objects that wish to show a dialog box Window for
// handling special protocols. The window that is displayed uses this interface
// to determine the text displayed and notify the delegate object of certain
// events.
class ProtocolDialogDelegate {
 public:
  explicit ProtocolDialogDelegate(const GURL& url) : url_(url) {}
  virtual ~ProtocolDialogDelegate() {}

  // Called if the user has chosen to launch the application for this protocol.
  // |dont_block| is true if the checkbox to prevent future instances of this
  // dialog is checked.
  virtual void DoAccept(const GURL& url, bool dont_block) const = 0;

  // Called if the user has chosen to do nothing for this protocol.
  // |dont_block| is true if the checkbox to prevent future instances of this
  // dialog is checked.
  virtual void DoCancel(const GURL& url, bool dont_block) const = 0;

  virtual base::string16 GetMessageText() const = 0;
  virtual base::string16 GetCheckboxText() const = 0;
  virtual base::string16 GetTitleText() const = 0;

  const GURL& url() const { return url_; }

 private:
  const GURL url_;
};

#endif  // CHROME_BROWSER_UI_PROTOCOL_DIALOG_DELEGATE_H_
