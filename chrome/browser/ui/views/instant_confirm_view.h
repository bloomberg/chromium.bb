// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INSTANT_CONFIRM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INSTANT_CONFIRM_VIEW_H_
#pragma once

#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

class Profile;

// The view shown in the instant confirm dialog.
class InstantConfirmView : public views::View,
                           public views::DialogDelegate,
                           public views::LinkController {
 public:
  explicit InstantConfirmView(Profile* profile);

  // DialogDelegate overrides:
  virtual bool Accept(bool window_closing);
  virtual bool Accept();
  virtual bool Cancel();
  virtual views::View* GetContentsView();
  virtual std::wstring GetWindowTitle() const;
  virtual gfx::Size GetPreferredSize();
  virtual bool IsModal() const;

  // LinkController overrides:
  virtual void LinkActivated(views::Link* source, int event_flags);

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InstantConfirmView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INSTANT_CONFIRM_VIEW_H_
