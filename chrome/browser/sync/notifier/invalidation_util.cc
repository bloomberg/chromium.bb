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

// We need to write our own protobuf to string functions because we
// use LITE_RUNTIME, which doesn't support DebugString().

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
  for (int i = 0; i < component_stamp_log.stamps_size(); ++i) {
    const invalidation::ComponentStamp& component_stamp =
        component_stamp_log.stamps(i);
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
