// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/message_center/notifier_settings.h"

class Profile;

namespace message_center {
struct Notifier;
}

// An interface to control Notifiers, grouped by NotifierType. Controllers are
// responsible for both collating display data and toggling settings in response
// to user inputs.
class NotifierController {
 public:
  class Observer {
   public:
    virtual void OnIconImageUpdated(const message_center::NotifierId& id,
                                    const gfx::Image& image) = 0;
    virtual void OnNotifierEnabledChanged(const message_center::NotifierId& id,
                                          bool enabled) = 0;
  };

  NotifierController() = default;
  virtual ~NotifierController() = default;

  // Returns notifiers to display in the settings UI. Not all notifiers appear
  // in settings. If the source starts loading for icon images, it needs to call
  // Observer::OnIconImageUpdated after the icon is loaded.
  virtual std::vector<std::unique_ptr<message_center::Notifier>>
  GetNotifierList(Profile* profile) = 0;

  // Set notifier enabled. |notifier_id| must have notifier type that can be
  // handled by the source. It has responsibility to invoke
  // Observer::OnNotifierEnabledChanged.
  virtual void SetNotifierEnabled(Profile* profile,
                                  const message_center::NotifierId& notifier_id,
                                  bool enabled) = 0;

  // Returns true if the given notifier should have an advanced settings button.
  virtual bool HasAdvancedSettings(
      Profile* profile,
      const message_center::NotifierId& notifier_id) const;

  // Called when the advanced settings button has been clicked.
  virtual void OnNotifierAdvancedSettingsRequested(
      Profile* profile,
      const message_center::NotifierId& notifier_id,
      const std::string* notification_id) {}

  // Release temporary resouces tagged with notifier list that is returned last
  // time.
  virtual void OnNotifierSettingsClosing() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NotifierController);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_CONTROLLER_H_
