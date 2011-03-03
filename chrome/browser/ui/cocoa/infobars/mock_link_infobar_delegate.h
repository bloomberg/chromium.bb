// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_MOCK_LINK_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_MOCK_LINK_INFOBAR_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/tab_contents/link_infobar_delegate.h"

class SkBitmap;

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
  virtual void InfoBarClosed() OVERRIDE;
  virtual SkBitmap* GetIcon() const OVERRIDE;
  virtual string16 GetMessageTextWithOffset(size_t* link_offset) const OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  // Determines whether the infobar closes when an action is taken or not.
  bool closes_on_action_;
  mutable bool icon_accessed_;
  mutable bool message_text_accessed_;
  mutable bool link_text_accessed_;
  bool link_clicked_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(MockLinkInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_MOCK_LINK_INFOBAR_DELEGATE_H_
