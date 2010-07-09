// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/invalidation_util.h"

#include <sstream>

namespace sync_notifier {

void RunAndDeleteClosure(invalidation::Closure* task) {
  task->Run();
  delete task;
}

bool RealModelTypeToObjectId(syncable::ModelType model_type,
                             invalidation::ObjectId* object_id) {
  std::string notification_type;
  if (!syncable::RealModelTypeToNotificationType(
          model_type, &notification_type)) {
    return false;
  }
  object_id->mutable_name()->set_string_value(notification_type);
  object_id->set_source(invalidation::ObjectId::CHROME_SYNC);
  return true;
}

bool ObjectIdToRealModelType(const invalidation::ObjectId& object_id,
                             syncable::ModelType* model_type) {
  return
      syncable::NotificationTypeToRealModelType(
          object_id.name().string_value(), model_type);
}

std::string ObjectIdToString(
    const invalidation::ObjectId& object_id) {
  std::stringstream ss;
  ss << "{ ";
  ss << "name: " << object_id.name().string_value() << ", ";
  ss << "source: " << object_id.source();
  ss << " }";
  return ss.str();
}

std::string StatusToString(
    const invalidation::Status& status) {
  std::stringstream ss;
  ss << "{ ";
  ss << "code: " << status.code() << ", ";
  ss << "description: " << status.description();
  ss << " }";
  return ss.str();
}

std::string InvalidationToString(
    const invalidation::Invalidation& invalidation) {
  std::stringstream ss;
  ss << "{ ";
  ss << "object_id: " << ObjectIdToString(invalidation.object_id()) << ", ";
  ss << "version: " << invalidation.version() << ", ";
  ss << "components: { ";
  const invalidation::ComponentStampLog& component_stamp_log =
      invalidation.component_stamp_log();
  for (int i = 0; i < component_stamp_log.stamp_size(); ++i) {
    const invalidation::ComponentStamp& component_stamp =
        component_stamp_log.stamp(i);
    ss << "component: " << component_stamp.component() << ", ";
    ss << "time: " << component_stamp.time() << ", ";
  }
  ss << " }";
  ss << " }";
  return ss.str();
}

std::string RegistrationUpdateToString(
    const invalidation::RegistrationUpdate& update) {
  std::stringstream ss;
  ss << "{ ";
  ss << "type: " << update.type() << ", ";
  ss << "object_id: " << ObjectIdToString(update.object_id()) << ", ";
  ss << "version: " << update.version() << ", ";
  ss << "sequence_number: " << update.sequence_number();
  ss << " }";
  return ss.str();
}

std::string RegistrationUpdateResultToString(
    const invalidation::RegistrationUpdateResult& update_result) {
  std::stringstream ss;
  ss << "{ ";
  ss << "operation: "
     << RegistrationUpdateToString(update_result.operation()) << ", ";
  ss << "status: " << StatusToString(update_result.status());
  ss << " }";
  return ss.str();
}

}  // namespace sync_notifier
