// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULER_MODEL_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULER_MODEL_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_types.h"

namespace notifications {

struct TypeState;
struct NotificationEntry;

// A model class which contains in-memory representations of scheduled
// notifications and impression data for a certain type of notification and
// provides functionalites to update these data.
class SchedulerModel {
 public:
  using InitCallback = base::OnceCallback<void(bool)>;

  // Initializes the model and load data into memory.
  virtual void Initialize(InitCallback init_callback) = 0;

  // Get the type state of a certain scheduler client. Returns nullptr when the
  // type state doesn't exist.
  virtual TypeState* GetTypeState(SchedulerClientType type) = 0;

  // Updates a type state. Uses GetTypeState() to modify data and call this to
  // commit the change to database.
  virtual void UpdateTypeState(SchedulerClientType type) = 0;

  // Deletes a type state.
  virtual void DeleteTypeState(SchedulerClientType type) = 0;

  // Adds a scheduled notification.
  virtual void AddNotification(NotificationEntry entry) = 0;

  // Gets a scheduled notification. Returns nullptr when the notification entry
  // doesn't exist.
  virtual NotificationEntry* GetNotification(const std::string& guid) = 0;

  // Updates a notification. Uses GetNotification() to modify data and call this
  // to commit the change to database.
  virtual void UpdateNotification(const std::string& guid) = 0;

  // Deletes a scheduled notification.
  virtual void DeleteNotification(const std::string& guid) = 0;

 protected:
  SchedulerModel() = default;
  virtual ~SchedulerModel() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerModel);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULER_MODEL_H_
