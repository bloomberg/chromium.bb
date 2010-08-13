// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/infobar_delegate.h"

#include "base/utf_string_conversions.h"

namespace {
const char kMockAlertInfoBarMessage[] = "MockAlertInfoBarMessage";
const char kMockLinkInfoBarMessage[] = "MockLinkInfoBarMessage";
const char kMockLinkInfoBarLink[] = "http://dev.chromium.org";
const char kMockConfirmInfoBarMessage[] = "MockConfirmInfoBarMessage";
}

//////////////////////////////////////////////////////////////////////////
// Mock InfoBarDelgates

class MockAlertInfoBarDelegate : public AlertInfoBarDelegate {
 public:
  explicit MockAlertInfoBarDelegate()
      : AlertInfoBarDelegate(NULL),
        message_text_accessed(false),
        icon_accessed(false),
        closed(false) {
  }

  virtual string16 GetMessageText() const {
    message_text_accessed = true;
    return ASCIIToUTF16(kMockAlertInfoBarMessage);
  }

  virtual SkBitmap* GetIcon() const {
    icon_accessed = true;
    return NULL;
  }

  virtual void InfoBarClosed() {
    closed = true;
  }

  // These are declared mutable to get around const-ness issues.
  mutable bool message_text_accessed;
  mutable bool icon_accessed;
  bool closed;
};

class MockLinkInfoBarDelegate : public LinkInfoBarDelegate {
 public:
  explicit MockLinkInfoBarDelegate()
      : LinkInfoBarDelegate(NULL),
        message_text_accessed(false),
        link_text_accessed(false),
        icon_accessed(false),
        link_clicked(false),
        closed(false),
        closes_on_action(true) {
  }

  virtual string16 GetMessageTextWithOffset(size_t* link_offset) const {
    message_text_accessed = true;
    return ASCIIToUTF16(kMockLinkInfoBarMessage);
  }

  virtual string16 GetLinkText() const {
    link_text_accessed = true;
    return ASCIIToUTF16(kMockLinkInfoBarLink);
  }

  virtual SkBitmap* GetIcon() const {
    icon_accessed = true;
    return NULL;
  }

  virtual bool LinkClicked(WindowOpenDisposition disposition) {
    link_clicked = true;
    return closes_on_action;
  }

  virtual void InfoBarClosed() {
    closed = true;
  }

  // These are declared mutable to get around const-ness issues.
  mutable bool message_text_accessed;
  mutable bool link_text_accessed;
  mutable bool icon_accessed;
  bool link_clicked;
  bool closed;

  // Determines whether the infobar closes when an action is taken or not.
  bool closes_on_action;
};

class MockConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit MockConfirmInfoBarDelegate()
      : ConfirmInfoBarDelegate(NULL),
        message_text_accessed(false),
        link_text_accessed(false),
        icon_accessed(false),
        ok_clicked(false),
        cancel_clicked(false),
        link_clicked(false),
        closed(false),
        closes_on_action(true) {
  }

  virtual int GetButtons() const {
    return (BUTTON_OK | BUTTON_CANCEL);
  }

  virtual string16 GetButtonLabel(InfoBarButton button) const {
    if (button == BUTTON_OK)
      return ASCIIToUTF16("OK");
    else
      return ASCIIToUTF16("Cancel");
  }

  virtual bool Accept() {
    ok_clicked = true;
    return closes_on_action;
  }

  virtual bool Cancel() {
    cancel_clicked = true;
    return closes_on_action;
  }

  virtual string16 GetMessageText() const {
    message_text_accessed = true;
    return ASCIIToUTF16(kMockConfirmInfoBarMessage);
  }

  virtual SkBitmap* GetIcon() const {
    icon_accessed = true;
    return NULL;
  }

  virtual void InfoBarClosed() {
    closed = true;
  }

  virtual string16 GetLinkText() {
    link_text_accessed = true;
    return string16();
  }

  virtual bool LinkClicked(WindowOpenDisposition disposition) {
    link_clicked = true;
    return closes_on_action;
  }

  // These are declared mutable to get around const-ness issues.
  mutable bool message_text_accessed;
  mutable bool link_text_accessed;
  mutable bool icon_accessed;
  bool ok_clicked;
  bool cancel_clicked;
  bool link_clicked;
  bool closed;

  // Determines whether the infobar closes when an action is taken or not.
  bool closes_on_action;
};
