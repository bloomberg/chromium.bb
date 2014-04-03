// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_LOCALE_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_LOCALE_LOCALE_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "ash/system/locale/locale_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {

// Observes the locale change and creates rich notification for the change.
class LocaleNotificationController : public LocaleObserver {
 public:
  LocaleNotificationController();
  virtual ~LocaleNotificationController();

 private:
  // Overridden from LocaleObserver.
  virtual void OnLocaleChanged(LocaleObserver::Delegate* delegate,
                               const std::string& cur_locale,
                               const std::string& from_locale,
                               const std::string& to_locale) OVERRIDE;

  LocaleObserver::Delegate* delegate_;
  std::string cur_locale_;
  std::string from_locale_;
  std::string to_locale_;

  DISALLOW_COPY_AND_ASSIGN(LocaleNotificationController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_LOCALE_NOTIFICATION_CONTROLLER_H_
