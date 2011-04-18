// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCALE_CHANGE_GUARD_H_
#define CHROME_BROWSER_CHROMEOS_LOCALE_CHANGE_GUARD_H_
#pragma once

#include <string>

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_type.h"

class ListValue;
class NotificationDetails;
class NotificationSource;
class Profile;

namespace chromeos {

// Performs check whether locale has been changed automatically recently
// (based on synchronized user preference).  If so: shows notification that
// allows user to revert change.
class LocaleChangeGuard : public NotificationObserver {
 public:
  explicit LocaleChangeGuard(Profile* profile);

  // Called just before changing locale.
  void PrepareChangingLocale(
      const std::string& from_locale, const std::string& to_locale);

  // Called after login.
  void OnLogin();

 private:
  class Delegate;

  void RevertLocaleChange(const ListValue* list);
  void AcceptLocaleChange();
  void Check();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  std::string from_locale_;
  std::string to_locale_;
  Profile* profile_;
  scoped_ptr<chromeos::SystemNotification> note_;
  bool reverted_;
  NotificationRegistrar registrar_;

  // We want to show locale change notification in previous language however
  // we cannot directly load strings for non-current locale.  So we cache
  // messages before locale change.
  string16 title_text_;
  string16 message_text_;
  string16 revert_link_text_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOCALE_CHANGE_GUARD_H_
