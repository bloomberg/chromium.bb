// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_ERROR_OBSERVER_H_
#define ASH_DISPLAY_DISPLAY_ERROR_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/display/manager/display_configurator.h"

namespace ash {

// The class to observe the output failures and shows the error dialog when
// necessary.
class ASH_EXPORT DisplayErrorObserver
    : public display::DisplayConfigurator::Observer {
 public:
  DisplayErrorObserver();
  ~DisplayErrorObserver() override;

  // display::DisplayConfigurator::Observer overrides:
  void OnDisplayModeChangeFailed(
      const display::DisplayConfigurator::DisplayStateList& displays,
      display::MultipleDisplayState failed_new_state) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayErrorObserver);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_ERROR_OBSERVER_H_
