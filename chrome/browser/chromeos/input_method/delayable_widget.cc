// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/delayable_widget.h"

#include "ui/views/widget/widget.h"

namespace chromeos {
namespace input_method {

DelayableWidget::DelayableWidget() {
}

DelayableWidget::~DelayableWidget() {
  show_hide_timer_.Stop();
}

void DelayableWidget::Show() {
  show_hide_timer_.Stop();
  views::Widget::Show();
}

void DelayableWidget::DelayShow(unsigned int milliseconds) {
  show_hide_timer_.Stop();
  show_hide_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(milliseconds),
                         this,
                         &DelayableWidget::Show);
}

void DelayableWidget::Hide() {
  show_hide_timer_.Stop();
  views::Widget::Hide();
}

void DelayableWidget::DelayHide(unsigned int milliseconds) {
  show_hide_timer_.Stop();
  show_hide_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(milliseconds),
                         this,
                         &DelayableWidget::Hide);
}

}  // namespace input_method
}  // namespace chromeos
