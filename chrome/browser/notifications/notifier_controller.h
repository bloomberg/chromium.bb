// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "base/macros.h"
#include "ui/message_center/notifier_id.h"

class Profile;

// An interface to control Notifiers, grouped by NotifierType. Controllers are
// responsible for both collating display data and toggling settings in response
// to user inputs.
class NotifierController {
 public:
  class Observer {
   public:
    virtual void OnIconImageUpdated(const message_center::NotifierId& id,
                                    const gfx::ImageSkia& image) = 0;
    virtual void OnNotifierEnabledChanged(const message_center::NotifierId& id,
                                          bool enabled) = 0;
  };

  NotifierController() = default;
  virtual ~NotifierController() = default;

  // Returns notifiers to display in the settings UI. Not all notifiers appear
  // in settings. If the source starts loading for icon images, it needs to call
  // Observer::OnIconImageUpdated after the icon is loaded.
  virtual std::vector<ash::mojom::NotifierUiDataPtr> GetNotifierList(
      Profile* profile) = 0;

  // Set notifier enabled. |notifier_id| must have notifier type that can be
  // handled by the source. It has responsibility to invoke
  // Observer::OnNotifierEnabledChanged.
  virtual void SetNotifierEnabled(Profile* profile,
                                  const message_center::NotifierId& notifier_id,
                                  bool enabled) = 0;

  // Called when the advanced settings button has been clicked.
  virtual void OnNotifierAdvancedSettingsRequested(
      Profile* profile,
      const message_center::NotifierId& notifier_id) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NotifierController);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_CONTROLLER_H_
