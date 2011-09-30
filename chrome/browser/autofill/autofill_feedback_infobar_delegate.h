// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_FEEDBACK_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_FEEDBACK_INFOBAR_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/tab_contents/link_infobar_delegate.h"
#include "webkit/glue/window_open_disposition.h"

class TabContents;

// An InfoBar delegate that prompts the user to provide additional feedback for
// the Autofill developers.
class AutofillFeedbackInfoBarDelegate : public LinkInfoBarDelegate {
 public:
  AutofillFeedbackInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                  const string16& message,
                                  const string16& link_text,
                                  const std::string& feedback_message);

 private:
  virtual ~AutofillFeedbackInfoBarDelegate();

  // LinkInfoBarDelegate:
  virtual string16 GetMessageTextWithOffset(size_t* link_offset) const OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  // The non-linked infobar text.
  const string16 message_;
  // The infobar link text.
  const string16 link_text_;
  // The default feedback message, which can be edited by the user prior to
  // submission.
  const std::string feedback_message_;

  bool link_clicked_;

  DISALLOW_COPY_AND_ASSIGN(AutofillFeedbackInfoBarDelegate);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_FEEDBACK_INFOBAR_DELEGATE_H_
