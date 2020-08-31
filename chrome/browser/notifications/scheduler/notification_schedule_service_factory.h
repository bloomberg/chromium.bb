// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace notifications {
class NotificationScheduleService;
}  // namespace notifications

class NotificationScheduleServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static NotificationScheduleServiceFactory* GetInstance();
  static notifications::NotificationScheduleService* GetForKey(
      SimpleFactoryKey* key);

 private:
  friend struct base::DefaultSingletonTraits<
      NotificationScheduleServiceFactory>;

  NotificationScheduleServiceFactory();
  ~NotificationScheduleServiceFactory() override;

  // SimpleKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;

  DISALLOW_COPY_AND_ASSIGN(NotificationScheduleServiceFactory);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_FACTORY_H_
