// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HANDLER_H_
#pragma once

#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

// Class that handles the accessibility notifications and generates
// appropriate spoken/audio feedback.
class WizardAccessibilityHandler : public NotificationObserver {
 public:
  // Speaks the specified string.
  void Speak(const char* speak_str, bool queue, bool interruptible);

 private:

  // Override from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HANDLER_H_
