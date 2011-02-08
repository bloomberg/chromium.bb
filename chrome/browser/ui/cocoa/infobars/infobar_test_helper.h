// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_TEST_HELPER_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_TEST_HELPER_H_
#pragma once

#include "chrome/browser/tab_contents/infobar_delegate.h"


// MockLinkInfoBarDelegate ----------------------------------------------------

class MockLinkInfoBarDelegate : public LinkInfoBarDelegate {
 public:
  MockLinkInfoBarDelegate();
  virtual ~MockLinkInfoBarDelegate();

  void set_dont_close_on_action() { closes_on_action_ = false; }

  bool icon_accessed() const { return icon_accessed_; }
  bool message_text_accessed() const { return message_text_accessed_; }
  bool link_text_accessed() const { return link_text_accessed_; }
  bool link_clicked() const { return link_clicked_; }
  bool closed() const { return closed_; }

  static const char kMessage[];
  static const char kLink[];

 private:
  // LinkInfoBarDelegate:
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual string16 GetMessageTextWithOffset(size_t* link_offset) const;
  virtual string16 GetLinkText() const;
  virtual bool LinkClicked(WindowOpenDisposition disposition);

  // Determines whether the infobar closes when an action is taken or not.
  bool closes_on_action_;

  mutable bool icon_accessed_;
  mutable bool message_text_accessed_;
  mutable bool link_text_accessed_;
  bool link_clicked_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(MockLinkInfoBarDelegate);
};


// MockConfirmInfoBarDelegate -------------------------------------------------

class MockConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  MockConfirmInfoBarDelegate();
  virtual ~MockConfirmInfoBarDelegate();

  void set_dont_close_on_action() { closes_on_action_ = false; }

  bool icon_accessed() const { return icon_accessed_; }
  bool message_text_accessed() const { return message_text_accessed_; }
  bool link_text_accessed() const { return link_text_accessed_; }
  bool ok_clicked() const { return ok_clicked_; }
  bool cancel_clicked() const { return cancel_clicked_; }
  bool link_clicked() const { return link_clicked_; }
  bool closed() const { return closed_; }

  static const char kMessage[];

 private:
  // ConfirmInfoBarDelegate:
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual string16 GetMessageText() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual string16 GetLinkText();
  virtual bool LinkClicked(WindowOpenDisposition disposition);

  // Determines whether the infobar closes when an action is taken or not.
  bool closes_on_action_;

  mutable bool icon_accessed_;
  mutable bool message_text_accessed_;
  mutable bool link_text_accessed_;
  bool ok_clicked_;
  bool cancel_clicked_;
  bool link_clicked_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(MockConfirmInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_TEST_HELPER_H_
