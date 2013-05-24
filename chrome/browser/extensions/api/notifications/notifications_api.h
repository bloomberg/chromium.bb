// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/notifications.h"
#include "ui/message_center/notification_types.h"

namespace extensions {

class NotificationsApiFunction : public ApiFunction {
 public:
  // Whether the current extension and channel allow the API. Public for
  // testing.
  bool IsNotificationsApiAvailable();

 protected:
  NotificationsApiFunction();
  virtual ~NotificationsApiFunction();

  void CreateNotification(
      const std::string& id,
      api::notifications::NotificationOptions* options);

  bool IsNotificationsApiEnabled();

  // Called inside of RunImpl.
  virtual bool RunNotificationsApi() = 0;

  // UITHreadExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  message_center::NotificationType MapApiTemplateTypeToType(
      api::notifications::TemplateType type);
};

class NotificationsCreateFunction : public NotificationsApiFunction {
 public:
  NotificationsCreateFunction();

  // UIThreadExtensionFunction:
  virtual bool RunNotificationsApi() OVERRIDE;

 protected:
  virtual ~NotificationsCreateFunction();

 private:
  scoped_ptr<api::notifications::Create::Params> params_;

  DECLARE_EXTENSION_FUNCTION("notifications.create", NOTIFICATIONS_CREATE)
};

class NotificationsUpdateFunction : public NotificationsApiFunction {
 public:
  NotificationsUpdateFunction();

  // UIThreadExtensionFunction:
  virtual bool RunNotificationsApi() OVERRIDE;

 protected:
  virtual ~NotificationsUpdateFunction();

 private:
  scoped_ptr<api::notifications::Update::Params> params_;

  DECLARE_EXTENSION_FUNCTION("notifications.update", NOTIFICATIONS_UPDATE)
};

class NotificationsClearFunction : public NotificationsApiFunction {
 public:
  NotificationsClearFunction();

  // UIThreadExtensionFunction:
  virtual bool RunNotificationsApi() OVERRIDE;

 protected:
  virtual ~NotificationsClearFunction();

 private:
  scoped_ptr<api::notifications::Clear::Params> params_;

  DECLARE_EXTENSION_FUNCTION("notifications.clear", NOTIFICATIONS_CLEAR)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_
