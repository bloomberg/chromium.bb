// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/registration_info.h"

#include <stddef.h>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace gcm {

namespace {
const char kInstanceIDSerializationPrefix[] = "iid-";
const int kInstanceIDSerializationPrefixLength =
    sizeof(kInstanceIDSerializationPrefix) / sizeof(char) - 1;
}  // namespace

// static
std::unique_ptr<RegistrationInfo> RegistrationInfo::BuildFromString(
    const std::string& serialized_key,
    const std::string& serialized_value,
    std::string* registration_id) {
  std::unique_ptr<RegistrationInfo> registration;

  if (base::StartsWith(serialized_key, kInstanceIDSerializationPrefix,
                       base::CompareCase::SENSITIVE))
    registration.reset(new InstanceIDTokenInfo);
  else
    registration.reset(new GCMRegistrationInfo);

  if (!registration->Deserialize(serialized_key,
                                 serialized_value,
                                 registration_id)) {
    registration.reset();
  }
  return registration;
}

RegistrationInfo::RegistrationInfo() {
}

RegistrationInfo::~RegistrationInfo() {
}

// static
const GCMRegistrationInfo* GCMRegistrationInfo::FromRegistrationInfo(
    const RegistrationInfo* registration_info) {
  if (!registration_info || registration_info->GetType() != GCM_REGISTRATION)
    return nullptr;
  return static_cast<const GCMRegistrationInfo*>(registration_info);
}

// static
GCMRegistrationInfo* GCMRegistrationInfo::FromRegistrationInfo(
    RegistrationInfo* registration_info) {
  if (!registration_info || registration_info->GetType() != GCM_REGISTRATION)
    return nullptr;
  return static_cast<GCMRegistrationInfo*>(registration_info);
}

GCMRegistrationInfo::GCMRegistrationInfo() {
}

GCMRegistrationInfo::~GCMRegistrationInfo() {
}

RegistrationInfo::RegistrationType GCMRegistrationInfo::GetType() const {
  return GCM_REGISTRATION;
}

std::string GCMRegistrationInfo::GetSerializedKey() const {
  // Multiple registrations are not supported for legacy GCM. So the key is
  // purely based on the application id.
  return app_id;
}

std::string GCMRegistrationInfo::GetSerializedValue(
    const std::string& registration_id) const {
  if (sender_ids.empty() || registration_id.empty())
    return std::string();

  // Serialize as:
  //    sender1,sender2,...=reg_id
  std::string value;
  for (std::vector<std::string>::const_iterator iter = sender_ids.begin();
       iter != sender_ids.end(); ++iter) {
    DCHECK(!iter->empty() &&
           iter->find(',') == std::string::npos &&
           iter->find('=') == std::string::npos);
    if (!value.empty())
      value += ",";
    value += *iter;
  }

  value += '=';
  value += registration_id;
  return value;
}

bool GCMRegistrationInfo::Deserialize(
    const std::string& serialized_key,
    const std::string& serialized_value,
    std::string* registration_id) {
  if (serialized_key.empty() || serialized_value.empty())
    return false;

  // Application ID is same as the serialized key.
  app_id = serialized_key;

  // Sender IDs and registration ID are constructed from the serialized value.
  size_t pos = serialized_value.find('=');
  if (pos == std::string::npos)
    return false;

  std::string senders = serialized_value.substr(0, pos);
  std::string registration_id_str = serialized_value.substr(pos + 1);

  sender_ids = base::SplitString(
      senders, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (sender_ids.empty() || registration_id_str.empty()) {
    sender_ids.clear();
    registration_id_str.clear();
    return false;
  }

  if (registration_id)
    *registration_id = registration_id_str;

  return true;
}

// static
const InstanceIDTokenInfo* InstanceIDTokenInfo::FromRegistrationInfo(
    const RegistrationInfo* registration_info) {
  if (!registration_info || registration_info->GetType() != INSTANCE_ID_TOKEN)
    return nullptr;
  return static_cast<const InstanceIDTokenInfo*>(registration_info);
}

// static
InstanceIDTokenInfo* InstanceIDTokenInfo::FromRegistrationInfo(
    RegistrationInfo* registration_info) {
  if (!registration_info || registration_info->GetType() != INSTANCE_ID_TOKEN)
    return nullptr;
  return static_cast<InstanceIDTokenInfo*>(registration_info);
}

InstanceIDTokenInfo::InstanceIDTokenInfo() {
}

InstanceIDTokenInfo::~InstanceIDTokenInfo() {
}

RegistrationInfo::RegistrationType InstanceIDTokenInfo::GetType() const {
  return INSTANCE_ID_TOKEN;
}

std::string InstanceIDTokenInfo::GetSerializedKey() const {
  DCHECK(app_id.find(',') == std::string::npos &&
         authorized_entity.find(',') == std::string::npos &&
         scope.find(',') == std::string::npos);

  // Multiple registrations are supported for Instance ID. So the key is based
  // on the combination of (app_id, authorized_entity, scope).

  // Adds a prefix to differentiate easily with GCM registration key.
  std::string key(kInstanceIDSerializationPrefix);
  key += app_id;
  key += ",";
  key += authorized_entity;
  key += ",";
  key += scope;
  return key;
}

std::string InstanceIDTokenInfo::GetSerializedValue(
    const std::string& registration_id) const {
  return registration_id;
}

bool InstanceIDTokenInfo::Deserialize(
    const std::string& serialized_key,
    const std::string& serialized_value,
    std::string* registration_id) {
  if (serialized_key.empty() || serialized_value.empty())
    return false;

  if (!base::StartsWith(serialized_key, kInstanceIDSerializationPrefix,
                        base::CompareCase::SENSITIVE))
    return false;

  std::vector<std::string> fields = base::SplitString(
      serialized_key.substr(kInstanceIDSerializationPrefixLength), ",",
      base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (fields.size() != 3 || fields[0].empty() ||
      fields[1].empty() || fields[2].empty()) {
    return false;
  }
  app_id = fields[0];
  authorized_entity = fields[1];
  scope = fields[2];

  // Registration ID is same as the serialized value;
  if (registration_id)
    *registration_id = serialized_value;

  return true;
}

bool RegistrationInfoComparer::operator()(
    const linked_ptr<RegistrationInfo>& a,
    const linked_ptr<RegistrationInfo>& b) const {
  DCHECK(a.get() && b.get());

  // For GCMRegistrationInfo, the comparison is based on app_id only.
  // For InstanceIDTokenInfo, the comparison is based on
  // <app_id, authorized_entity, scope>.
  if (a->app_id < b->app_id)
    return true;
  if (a->app_id > b->app_id)
    return false;

  InstanceIDTokenInfo* iid_a =
      InstanceIDTokenInfo::FromRegistrationInfo(a.get());
  InstanceIDTokenInfo* iid_b =
    InstanceIDTokenInfo::FromRegistrationInfo(b.get());

  // !iid_a && !iid_b => false.
  // !iid_a && iid_b => true.
  // This makes GCM record is sorted before InstanceID record.
  if (!iid_a)
    return iid_b != nullptr;

  // iid_a && !iid_b => false.
  if (!iid_b)
    return false;

  // Otherwise, compare with authorized_entity and scope.
  if (iid_a->authorized_entity < iid_b->authorized_entity)
    return true;
  if (iid_a->authorized_entity > iid_b->authorized_entity)
    return false;
  return iid_a->scope < iid_b->scope;
}

bool ExistsGCMRegistrationInMap(const RegistrationInfoMap& map,
                                const std::string& app_id) {
  std::unique_ptr<GCMRegistrationInfo> gcm_registration(
      new GCMRegistrationInfo);
  gcm_registration->app_id = app_id;
  return map.count(
      make_linked_ptr<RegistrationInfo>(gcm_registration.release())) > 0;
}

}  // namespace gcm
