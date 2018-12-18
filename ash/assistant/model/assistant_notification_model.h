// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_NOTIFICATION_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_NOTIFICATION_MODEL_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

namespace ash {

class AssistantNotificationModelObserver;

// The model belonging to AssistantNotificationController which tracks
// notification state and notifies a pool of observers.
class AssistantNotificationModel {
 public:
  using AssistantNotification =
      chromeos::assistant::mojom::AssistantNotification;
  using AssistantNotificationPtr =
      chromeos::assistant::mojom::AssistantNotificationPtr;

  AssistantNotificationModel();
  ~AssistantNotificationModel();

  // Adds/removes the specified notification model |observer|.
  void AddObserver(AssistantNotificationModelObserver* observer);
  void RemoveObserver(AssistantNotificationModelObserver* observer);

  // Adds the specified |notification| to the model.
  void AddNotification(AssistantNotificationPtr notification);

  // Removes the notification uniquely identified by |id|. If |from_server| is
  // true the request to remove was initiated by the server.
  void RemoveNotificationById(const std::string& id, bool from_server);

  // Removes the notifications identified by |grouping_key|. If |from_server| is
  // true the request to remove was initiated by the server.
  void RemoveNotificationsByGroupingKey(const std::string& grouping_key,
                                        bool from_server);

  // Removes all notifications. If |from_server| is true the request to remove
  // was initiated by the server.
  void RemoveAllNotifications(bool from_server);

  // Returns the notification uniquely identified by |id|.
  const AssistantNotification* GetNotificationById(const std::string& id) const;

 private:
  void NotifyNotificationAdded(const AssistantNotification* notification);
  void NotifyNotificationRemoved(const AssistantNotification* notification,
                                 bool from_server);
  void NotifyAllNotificationsRemoved(bool from_server);

  // Notifications are each mapped to their unique id.
  std::map<std::string, AssistantNotificationPtr> notifications_;

  base::ObserverList<AssistantNotificationModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_NOTIFICATION_MODEL_H_
