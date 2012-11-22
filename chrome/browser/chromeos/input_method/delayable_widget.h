// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_DELAYABLE_WIDGET_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_DELAYABLE_WIDGET_H_

#include "base/timer.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace input_method {

// This class implements widget which can show/hide with given delay.
class DelayableWidget : public views::Widget {
 public:
  DelayableWidget();
  virtual ~DelayableWidget();

  // Shows window.
  virtual void Show() OVERRIDE;

  // Shows window after |milliseconds| milli secs. This function can be called
  // in the DelayShow/DelayHide delay duration. In such case, previous operation
  // will be cancelled.
  void DelayShow(unsigned int milliseconds);

  // Hides window.
  void Hide();

  // Hides window after |milliseconds| milli secs. This function can be called
  // in the DelayShow/DelayHide delay duration. In such case, previous operation
  // will be cancelled.
  void DelayHide(unsigned int milliseconds);

 private:
  base::OneShotTimer<DelayableWidget> show_hide_timer_;

  DISALLOW_COPY_AND_ASSIGN(DelayableWidget);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_DELAYABLE_WIDGET_H_
