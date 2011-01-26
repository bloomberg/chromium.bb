// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/infobars/infobar_test_helper.h"

#include "base/utf_string_conversions.h"


// MockLinkInfoBarDelegate ----------------------------------------------------

const char MockLinkInfoBarDelegate::kMessage[] = "MockLinkInfoBarMessage";
const char MockLinkInfoBarDelegate::kLink[] = "http://dev.chromium.org";

MockLinkInfoBarDelegate::MockLinkInfoBarDelegate()
    : LinkInfoBarDelegate(NULL),
      closes_on_action_(true),
      icon_accessed_(false),
      message_text_accessed_(false),
      link_text_accessed_(false),
      link_clicked_(false),
      closed_(false) {
}

MockLinkInfoBarDelegate::~MockLinkInfoBarDelegate() {
}

void MockLinkInfoBarDelegate::InfoBarClosed() {
  closed_ = true;
}

SkBitmap* MockLinkInfoBarDelegate::GetIcon() const {
  icon_accessed_ = true;
  return NULL;
}

string16 MockLinkInfoBarDelegate::GetMessageTextWithOffset(
    size_t* link_offset) const {
  message_text_accessed_ = true;
  return ASCIIToUTF16(kMessage);
}

string16 MockLinkInfoBarDelegate::GetLinkText() const {
  link_text_accessed_ = true;
  return ASCIIToUTF16(kLink);
}

bool MockLinkInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  link_clicked_ = true;
  return closes_on_action_;
}


// MockConfirmInfoBarDelegate -------------------------------------------------

const char MockConfirmInfoBarDelegate::kMessage[] = "MockConfirmInfoBarMessage";

MockConfirmInfoBarDelegate::MockConfirmInfoBarDelegate()
    : ConfirmInfoBarDelegate(NULL),
      closes_on_action_(true),
      icon_accessed_(false),
      message_text_accessed_(false),
      link_text_accessed_(false),
      ok_clicked_(false),
      cancel_clicked_(false),
      link_clicked_(false),
      closed_(false) {
}

MockConfirmInfoBarDelegate::~MockConfirmInfoBarDelegate() {
}

void MockConfirmInfoBarDelegate::InfoBarClosed() {
  closed_ = true;
}

SkBitmap* MockConfirmInfoBarDelegate::GetIcon() const {
  icon_accessed_ = true;
  return NULL;
}

string16 MockConfirmInfoBarDelegate::GetMessageText() const {
  message_text_accessed_ = true;
  return ASCIIToUTF16(kMessage);
}

string16 MockConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return ASCIIToUTF16((button == BUTTON_OK) ? "OK" : "Cancel");
}

bool MockConfirmInfoBarDelegate::Accept() {
  ok_clicked_ = true;
  return closes_on_action_;
}

bool MockConfirmInfoBarDelegate::Cancel() {
  cancel_clicked_ = true;
  return closes_on_action_;
}

string16 MockConfirmInfoBarDelegate::GetLinkText() {
  link_text_accessed_ = true;
  return string16();
}

bool MockConfirmInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  link_clicked_ = true;
  return closes_on_action_;
}
