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
  object_id->Init(invalidation::ObjectSource::CHROME_SYNC, notification_type);
  return true;
}

bool ObjectIdToRealModelType(const invalidation::ObjectId& object_id,
                             syncable::ModelType* model_type) {
  return
      syncable::NotificationTypeToRealModelType(
          object_id.name(), model_type);
}

std::string ObjectIdToString(
    const invalidation::ObjectId& object_id) {
  std::stringstream ss;
  ss << "{ ";
  ss << "name: " << object_id.name() << ", ";
  ss << "source: " << object_id.source();
  ss << " }";
  return ss.str();
}

std::string ObjectIdPToString(
    const invalidation::ObjectIdP& object_id) {
  return ObjectIdToString(
      invalidation::ObjectId(
          (invalidation::ObjectSource_Type) object_id.source(),
          object_id.name().string_value()));
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
  ss << "version: " << invalidation.version();
  ss << " }";
  return ss.str();
}

std::string RegistrationUpdateToString(
    const invalidation::RegistrationUpdate& update) {
  std::stringstream ss;
  ss << "{ ";
  ss << "type: " << update.type() << ", ";
  ss << "object_id: " << ObjectIdPToString(update.object_id()) << ", ";
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
