// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_feedback_infobar_delegate.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/bug_report_ui.h"
#include "chrome/browser/userfeedback/proto/extension.pb.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

AutofillFeedbackInfoBarDelegate::AutofillFeedbackInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    const string16& message,
    const string16& link_text,
    const std::string& feedback_message)
    : LinkInfoBarDelegate(infobar_helper),
      message_(message),
      link_text_(link_text),
      feedback_message_(feedback_message),
      link_clicked_(false) {
}

AutofillFeedbackInfoBarDelegate::~AutofillFeedbackInfoBarDelegate() {
}

string16 AutofillFeedbackInfoBarDelegate::GetMessageTextWithOffset(
    size_t* link_offset) const {
  string16 message = message_ + ASCIIToUTF16(" ");
  *link_offset = message.size();
  return message;
}

string16 AutofillFeedbackInfoBarDelegate::GetLinkText() const {
  return link_text_;
}

bool AutofillFeedbackInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
#if defined(OS_CHROMEOS)
  size_t issue_type = userfeedback::ChromeOsData_ChromeOsCategory_AUTOFILL;
#else
  size_t issue_type =
      userfeedback::ChromeBrowserData_ChromeBrowserCategory_AUTOFILL;
#endif

  browser::ShowHtmlBugReportView(
      Browser::GetBrowserForController(&owner()->tab_contents()->controller(),
                                       NULL),
      feedback_message_,
      issue_type);
  return true;
}
