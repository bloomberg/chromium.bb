// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/views/lock_screen.h"

#include "ash/login/views/lock_contents_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

void ShowLockScreenInWidget(views::Widget* widget) {
  views::View* contents = new LockContentsView();
  widget->SetContentsView(contents);
  widget->Show();
}

}  // namespace ash
