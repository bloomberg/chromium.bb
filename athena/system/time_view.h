// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SYSTEM_TIME_VIEW_H_
#define ATHENA_SYSTEM_TIME_VIEW_H_

#include "athena/system/public/system_ui.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/controls/label.h"

namespace athena {

// View which displays the current time.
class TimeView : public views::Label {
 public:
  explicit TimeView(SystemUI::ColorScheme color_scheme);
  virtual ~TimeView();

 private:
  // Starts |timer_| to schedule the next update.
  void SetTimer(base::Time now);

  // Updates the current time.
  void UpdateText();

  base::OneShotTimer<TimeView> timer_;

  DISALLOW_COPY_AND_ASSIGN(TimeView);
};

}  // namespace athena

#endif  // ATHENA_SYSTEM_TIME_VIEW_H_
