// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_

#include <vector>

#include "base/basictypes.h"
#include "ui/message_center/notifier_settings_view_delegate.h"

namespace message_center {
class NotifierSettingsView;
}

// The class to bridge between the settings UI of notifiers and the preference
// storage.
class MessageCenterSettingsController
    : public message_center::NotifierSettingsViewDelegate {
 public:
  MessageCenterSettingsController();
  virtual ~MessageCenterSettingsController();

  // Shows a new settings dialog window with specified |context|. If there's
  // already an existing dialog, it raises the dialog instead of creating a new
  // one.
  void ShowSettingsDialog(gfx::NativeView context);

  // Overridden from message_center::NotifierSettingsViewDelegate.
  virtual void GetNotifierList(
      std::vector<message_center::Notifier*>* notifiers)
      OVERRIDE;
  virtual void SetNotifierEnabled(
      const message_center::Notifier& notifier,
      bool enabled) OVERRIDE;
  virtual void OnNotifierSettingsClosing() OVERRIDE;

 private:
  // The view displaying notifier settings. NULL if the settings are not
  // visible.
  message_center::NotifierSettingsView* settings_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterSettingsController);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
