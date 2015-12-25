// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database_data_conversions.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/notifications/notification_database_data.pb.h"
#include "content/public/browser/notification_database_data.h"

namespace content {

bool DeserializeNotificationDatabaseData(const std::string& input,
                                         NotificationDatabaseData* output) {
  DCHECK(output);

  NotificationDatabaseDataProto message;
  if (!message.ParseFromString(input))
    return false;

  output->notification_id = message.notification_id();
  output->origin = GURL(message.origin());
  output->service_worker_registration_id =
      message.service_worker_registration_id();

  PlatformNotificationData* notification_data = &output->notification_data;
  const NotificationDatabaseDataProto::NotificationData& payload =
      message.notification_data();

  notification_data->title = base::UTF8ToUTF16(payload.title());

  switch (payload.direction()) {
    case NotificationDatabaseDataProto::NotificationData::LEFT_TO_RIGHT:
      notification_data->direction =
          PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT;
      break;
    case NotificationDatabaseDataProto::NotificationData::RIGHT_TO_LEFT:
      notification_data->direction =
          PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT;
      break;
    case NotificationDatabaseDataProto::NotificationData::AUTO:
      notification_data->direction = PlatformNotificationData::DIRECTION_AUTO;
      break;
  }

  notification_data->lang = payload.lang();
  notification_data->body = base::UTF8ToUTF16(payload.body());
  notification_data->tag = payload.tag();
  notification_data->icon = GURL(payload.icon());

  if (payload.vibration_pattern().size() > 0) {
    notification_data->vibration_pattern.assign(
        payload.vibration_pattern().begin(), payload.vibration_pattern().end());
  }

  notification_data->silent = payload.silent();
  notification_data->require_interaction = payload.require_interaction();

  if (payload.data().length()) {
    notification_data->data.assign(payload.data().begin(),
                                   payload.data().end());
  }

  for (const auto& payload_action : payload.actions()) {
    PlatformNotificationAction action;
    action.action = payload_action.action();
    action.title = base::UTF8ToUTF16(payload_action.title());
    notification_data->actions.push_back(action);
  }

  return true;
}

bool SerializeNotificationDatabaseData(const NotificationDatabaseData& input,
                                       std::string* output) {
  DCHECK(output);

  scoped_ptr<NotificationDatabaseDataProto::NotificationData> payload(
      new NotificationDatabaseDataProto::NotificationData());

  const PlatformNotificationData& notification_data = input.notification_data;

  payload->set_title(base::UTF16ToUTF8(notification_data.title));

  switch (notification_data.direction) {
    case PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT:
      payload->set_direction(
          NotificationDatabaseDataProto::NotificationData::LEFT_TO_RIGHT);
      break;
    case PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT:
      payload->set_direction(
          NotificationDatabaseDataProto::NotificationData::RIGHT_TO_LEFT);
      break;
    case PlatformNotificationData::DIRECTION_AUTO:
      payload->set_direction(
          NotificationDatabaseDataProto::NotificationData::AUTO);
      break;
  }

  payload->set_lang(notification_data.lang);
  payload->set_body(base::UTF16ToUTF8(notification_data.body));
  payload->set_tag(notification_data.tag);
  payload->set_icon(notification_data.icon.spec());

  for (size_t i = 0; i < notification_data.vibration_pattern.size(); ++i)
    payload->add_vibration_pattern(notification_data.vibration_pattern[i]);

  payload->set_silent(notification_data.silent);
  payload->set_require_interaction(notification_data.require_interaction);

  if (notification_data.data.size()) {
    payload->set_data(&notification_data.data.front(),
                      notification_data.data.size());
  }

  for (const PlatformNotificationAction& action : notification_data.actions) {
    NotificationDatabaseDataProto::NotificationAction* payload_action =
        payload->add_actions();
    payload_action->set_action(action.action);
    payload_action->set_title(base::UTF16ToUTF8(action.title));
  }

  NotificationDatabaseDataProto message;
  message.set_notification_id(input.notification_id);
  message.set_origin(input.origin.spec());
  message.set_service_worker_registration_id(
      input.service_worker_registration_id);
  message.set_allocated_notification_data(payload.release());

  return message.SerializeToString(output);
}

}  // namespace content
