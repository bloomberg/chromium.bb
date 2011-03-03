// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/infobars/mock_link_infobar_delegate.h"

#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"

const char MockLinkInfoBarDelegate::kMessage[] = "MockLinkInfoBarMessage ";
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
  *link_offset = arraysize(kMessage) - 1;
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
