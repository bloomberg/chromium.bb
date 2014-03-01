// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTERNAL_PROTOCOL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTERNAL_PROTOCOL_DIALOG_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "chrome/browser/ui/protocol_dialog_delegate.h"
#include "url/gurl.h"

// Provides text for the external protocol handler dialog and handles whether
// or not to launch the application for the given protocol.
class ExternalProtocolDialogDelegate : public ProtocolDialogDelegate {
 public:
  explicit ExternalProtocolDialogDelegate(const GURL& url,
                                          int render_process_host_id,
                                          int tab_contents_id);
  virtual ~ExternalProtocolDialogDelegate();

  const base::string16& program_name() const { return program_name_; }

  virtual void DoAccept(const GURL& url, bool dont_block) const OVERRIDE;
  virtual void DoCancel(const GURL& url, bool dont_block) const OVERRIDE;

  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetCheckboxText() const OVERRIDE;
  virtual base::string16 GetTitleText() const OVERRIDE;

 private:
  int render_process_host_id_;
  int tab_contents_id_;
  const base::string16 program_name_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolDialogDelegate);
};

#endif  // CHROME_BROWSER_UI_EXTERNAL_PROTOCOL_DIALOG_DELEGATE_H_
