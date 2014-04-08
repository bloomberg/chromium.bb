// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_ERROR_OBSERVER_CHROMEOS_H_
#define ASH_DISPLAY_DISPLAY_ERROR_OBSERVER_CHROMEOS_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/display/chromeos/display_configurator.h"

namespace ash {

// The class to observe the output failures and shows the error dialog when
// necessary.
class ASH_EXPORT DisplayErrorObserver
    : public ui::DisplayConfigurator::Observer {
 public:
  DisplayErrorObserver();
  virtual ~DisplayErrorObserver();

  // ui::DisplayConfigurator::Observer overrides:
  virtual void OnDisplayModeChangeFailed(
      ui::MultipleDisplayState failed_new_state) OVERRIDE;

 private:
  friend class DisplayErrorObserverTest;

  base::string16 GetTitleOfDisplayErrorNotificationForTest();

  DISALLOW_COPY_AND_ASSIGN(DisplayErrorObserver);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_ERROR_OBSERVER_CHROMEOS_H_
