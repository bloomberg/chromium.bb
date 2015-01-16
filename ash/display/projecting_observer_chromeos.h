// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_PROJECTING_OBSERVER_CHROMEOS_H_
#define ASH_DISPLAY_PROJECTING_OBSERVER_CHROMEOS_H_

#include "ash/ash_export.h"
#include "ui/display/chromeos/display_configurator.h"

namespace chromeos {
class PowerManagerClient;
}

namespace ash {

class ASH_EXPORT ProjectingObserver : public ui::DisplayConfigurator::Observer {
 public:
  // |power_manager_client| must outlive this object.
  explicit ProjectingObserver(
      chromeos::PowerManagerClient* power_manager_client);
  ~ProjectingObserver() override;

  // Called when a casting session is started or stopped.
  void OnCastingSessionStartedOrStopped(bool started);

  // DisplayConfigurator::Observer implementation:
  void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& outputs) override;

 private:
  // Sends the current projecting state to power manager.
  void SetIsProjecting();

  // True if at least one output is internal. This value is updated when
  // |OnDisplayModeChanged| is called.
  bool has_internal_output_;

  // Keeps track of the number of connected outputs.
  int output_count_;

  // Number of outstanding casting sessions.
  int casting_session_count_;

  // Weak pointer to the DBusClient PowerManagerClient;
  chromeos::PowerManagerClient* power_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(ProjectingObserver);
};

}  // namespace ash

#endif  // ASH_DISPLAY_PROJECTING_OBSERVER_CHROMEOS_H_
