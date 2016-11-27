// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_SOURCE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_SOURCE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/message_center/notifier_settings.h"

class Profile;

namespace message_center {
struct Notifier;
}

class NotifierSource {
 public:
  class Observer {
   public:
    virtual void OnIconImageUpdated(const message_center::NotifierId& id,
                                    const gfx::Image& image) = 0;
    virtual void OnNotifierEnabledChanged(const message_center::NotifierId& id,
                                          bool enabled) = 0;
  };

  NotifierSource() = default;
  virtual ~NotifierSource() = default;

  // Returns notifiers.
  // If the source starts loading for icon images, it needs to call
  // Observer::OnIconImageUpdated after the icon is loaded.
  virtual std::vector<std::unique_ptr<message_center::Notifier>>
  GetNotifierList(Profile* profile) = 0;

  // Set notifier enabled. |notifier| must have notifier type that can be
  // handled by the source. It has responsibility to invoke
  // Observer::OnNotifierEnabledChanged.
  virtual void SetNotifierEnabled(Profile* profile,
                                  const message_center::Notifier& notifier,
                                  bool enabled) = 0;

  // Release temporary resouces tagged with notifier list that is returned last
  // time.
  virtual void OnNotifierSettingsClosing() {}

  // Notifier type provided by the source.
  virtual message_center::NotifierId::NotifierType GetNotifierType() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotifierSource);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_SOURCE_H_
