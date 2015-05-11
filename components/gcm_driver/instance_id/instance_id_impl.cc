// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id_impl.h"

#include <algorithm>
#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "crypto/random.h"

namespace instance_id {

// static
InstanceID* InstanceID::Create(const std::string& app_id,
                               gcm::GCMDriver* gcm_driver) {
  return new InstanceIDImpl(app_id, gcm_driver);
}

InstanceIDImpl::InstanceIDImpl(const std::string& app_id,
                               gcm::GCMDriver* gcm_driver)
    : InstanceID(app_id),
      gcm_driver_(gcm_driver),
      load_from_store_(false),
      weak_ptr_factory_(this) {
  gcm_driver_->GetInstanceIDStore()->GetInstanceIDData(
      app_id,
      base::Bind(&InstanceIDImpl::GetInstanceIDDataCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

InstanceIDImpl::~InstanceIDImpl() {
}

void InstanceIDImpl::GetID(const GetIDCallback& callback) {
  if (!delayed_task_controller_.CanRunTaskWithoutDelay()) {
    delayed_task_controller_.AddTask(
        base::Bind(&InstanceIDImpl::DoGetID,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
    return;
  }

  DoGetID(callback);
}

void InstanceIDImpl::DoGetID(const GetIDCallback& callback) {
  EnsureIDGenerated();
  callback.Run(id_);
}

void InstanceIDImpl::GetCreationTime(const GetCreationTimeCallback& callback) {
  if (!delayed_task_controller_.CanRunTaskWithoutDelay()) {
    delayed_task_controller_.AddTask(
        base::Bind(&InstanceIDImpl::DoGetCreationTime,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
    return;
  }

  DoGetCreationTime(callback);
}

void InstanceIDImpl::DoGetCreationTime(
    const GetCreationTimeCallback& callback) {
  callback.Run(creation_time_);
}

void InstanceIDImpl::GetToken(
    const std::string& authorized_entity,
    const std::string& scope,
    const std::map<std::string, std::string>& options,
    const GetTokenCallback& callback) {
  NOTIMPLEMENTED();
}

void InstanceIDImpl::DeleteToken(const std::string& authorized_entity,
                                 const std::string& scope,
                                 const DeleteTokenCallback& callback) {
  NOTIMPLEMENTED();
}

void InstanceIDImpl::DeleteID(const DeleteIDCallback& callback) {
  gcm_driver_->GetInstanceIDStore()->RemoveInstanceIDData(app_id());

  id_.clear();
  creation_time_ = base::Time();

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, InstanceID::SUCCESS));
}

void InstanceIDImpl::GetInstanceIDDataCompleted(
    const std::string& instance_id_data) {
  Deserialize(instance_id_data);
  delayed_task_controller_.SetReady();
}

void InstanceIDImpl::EnsureIDGenerated() {
  if (!id_.empty())
    return;

  // Now produce the ID in the following steps:

  // 1) Generates the random number in 8 bytes which is required by the server.
  //    We don't want to be strictly cryptographically secure. The server might
  //    reject the ID if there is a conflict or problem.
  uint8 bytes[kInstanceIDByteLength];
  crypto::RandBytes(bytes, sizeof(bytes));

  // 2) Transforms the first 4 bits to 0x7. Note that this is required by the
  //    server.
  bytes[0] &= 0x0f;
  bytes[0] |= 0x70;

  // 3) Encode the value in Android-compatible base64 scheme:
  //    * URL safe: '/' replaced by '_' and '+' replaced by '-'.
  //    * No padding: any trailing '=' will be removed.
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(bytes), sizeof(bytes)),
      &id_);
  std::replace(id_.begin(), id_.end(), '+', '-');
  std::replace(id_.begin(), id_.end(), '/', '_');
  id_.erase(std::remove(id_.begin(), id_.end(), '='), id_.end());

  creation_time_ = base::Time::Now();

  // Save to the persistent store.
  gcm_driver_->GetInstanceIDStore()->AddInstanceIDData(
      app_id(), SerializeAsString());
}

std::string InstanceIDImpl::SerializeAsString() const {
  std::string serialized_data;
  serialized_data += id_;
  serialized_data += ",";
  serialized_data += base::Int64ToString(creation_time_.ToInternalValue());
  return serialized_data;
}

void InstanceIDImpl::Deserialize(const std::string& serialized_data) {
  if (serialized_data.empty())
    return;
  std::size_t pos = serialized_data.find(',');
  if (pos == std::string::npos) {
    DVLOG(1) << "Failed to deserialize the InstanceID data: " + serialized_data;
    return;
  }

  id_ = serialized_data.substr(0, pos);

  int64 time_internal = 0LL;
  if (!base::StringToInt64(serialized_data.substr(pos + 1), &time_internal)) {
    DVLOG(1) << "Failed to deserialize the InstanceID data: " + serialized_data;
    return;
  }
  creation_time_ = base::Time::FromInternalValue(time_internal);
}

}  // namespace instance_id
