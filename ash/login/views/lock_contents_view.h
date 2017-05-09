// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_VIEWS_CONTENTS_VIEW_H_
#define ASH_LOGIN_VIEWS_CONTENTS_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/view.h"

namespace ash {

// LockContentsView hosts the root view for the lock screen. All other lock
// screen views are embedded within this one. LockContentsView is per-display,
// but the lock screen should only ever be shown on a single display, so there
// is effectively only a single instance.
class LockContentsView : public views::View, public views::ButtonListener {
 public:
  LockContentsView();
  ~LockContentsView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  views::Button* unlock_button_;

  DISALLOW_COPY_AND_ASSIGN(LockContentsView);
};

}  // namespace ash

#endif  // ASH_LOGIN_VIEWS_CONTENTS_VIEW_H_
