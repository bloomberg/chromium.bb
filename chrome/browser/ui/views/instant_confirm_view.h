// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INSTANT_CONFIRM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INSTANT_CONFIRM_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

// The view shown in the instant confirm dialog.
class InstantConfirmView : public views::DialogDelegateView,
                           public views::LinkListener {
 public:
  explicit InstantConfirmView(Profile* profile);

  // views::DialogDelegate overrides:
  virtual bool Accept(bool window_closing) OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual bool IsModal() const OVERRIDE;

  // views::LinkListener overrides:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InstantConfirmView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INSTANT_CONFIRM_VIEW_H_
