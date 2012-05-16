// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_feedback_infobar_delegate.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/feedback_ui.h"
#include "chrome/browser/feedback/proto/extension.pb.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

const char kCategoryTagAutofill[] = "Autofill";

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
  browser::ShowWebFeedbackView(
      browser::FindBrowserForController(
          &owner()->web_contents()->GetController(), NULL),
      feedback_message_,
      std::string(kCategoryTagAutofill));
  return true;
}
