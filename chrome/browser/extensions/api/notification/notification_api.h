// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NOTIFICATION_NOTIFICATION_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_NOTIFICATION_NOTIFICATION_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/experimental_notification.h"
#include "ui/message_center/notification_types.h"

namespace extensions {

class NotificationApiFunction : public ApiFunction {
 protected:
  NotificationApiFunction();
  virtual ~NotificationApiFunction();

  void CreateNotification(
      const std::string& id,
      api::experimental_notification::NotificationOptions* options);

  bool IsNotificationApiEnabled();

  // Called inside of RunImpl.
  virtual bool RunNotificationApi() = 0;

  // UITHreadExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  message_center::NotificationType MapApiTemplateTypeToType(
      api::experimental_notification::TemplateType type);
};

class NotificationCreateFunction : public NotificationApiFunction {
 public:
  NotificationCreateFunction();

  // UIThreadExtensionFunction:
  virtual bool RunNotificationApi() OVERRIDE;

 protected:
  virtual ~NotificationCreateFunction();

 private:
  scoped_ptr<api::experimental_notification::Create::Params> params_;

  DECLARE_EXTENSION_FUNCTION("experimental.notification.create",
                             EXPERIMENTAL_NOTIFICATION_CREATE)
};

class NotificationUpdateFunction : public NotificationApiFunction {
 public:
  NotificationUpdateFunction();

  // UIThreadExtensionFunction:
  virtual bool RunNotificationApi() OVERRIDE;

 protected:
  virtual ~NotificationUpdateFunction();

 private:
  scoped_ptr<api::experimental_notification::Update::Params> params_;

  DECLARE_EXTENSION_FUNCTION("experimental.notification.update",
                             EXPERIMENTAL_NOTIFICATION_UPDATE)
};

class NotificationClearFunction : public NotificationApiFunction {
 public:
  NotificationClearFunction();

  // UIThreadExtensionFunction:
  virtual bool RunNotificationApi() OVERRIDE;

 protected:
  virtual ~NotificationClearFunction();

 private:
  scoped_ptr<api::experimental_notification::Clear::Params> params_;

  DECLARE_EXTENSION_FUNCTION("experimental.notification.clear",
                             EXPERIMENTAL_NOTIFICATION_CLEAR)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NOTIFICATION_NOTIFICATION_API_H_
