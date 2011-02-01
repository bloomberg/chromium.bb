// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_policy_cache.h"

#include <limits>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

namespace policy {

// Saves policy information to a file.
class PersistPolicyTask : public Task {
 public:
  PersistPolicyTask(const FilePath& path,
                    const em::DevicePolicyResponse* policy,
                    const base::Time& timestamp,
                    const bool is_device_unmanaged)
      : path_(path),
        policy_(policy),
        timestamp_(timestamp),
        is_device_unmanaged_(is_device_unmanaged) {}

 private:
  // Task override.
  virtual void Run();

  const FilePath path_;
  scoped_ptr<const em::DevicePolicyResponse> policy_;
  const base::Time timestamp_;
  const bool is_device_unmanaged_;
};

void PersistPolicyTask::Run() {
  std::string data;
  em::CachedDevicePolicyResponse cached_policy;
  if (policy_.get())
    cached_policy.mutable_policy()->CopyFrom(*policy_);
  if (is_device_unmanaged_)
    cached_policy.set_unmanaged(true);
  cached_policy.set_timestamp(timestamp_.ToInternalValue());
  if (!cached_policy.SerializeToString(&data)) {
    LOG(WARNING) << "Failed to serialize policy data";
    return;
  }

  int size = data.size();
  if (file_util::WriteFile(path_, data.c_str(), size) != size) {
    LOG(WARNING) << "Failed to write " << path_.value();
    return;
  }
}

DeviceManagementPolicyCache::DeviceManagementPolicyCache(
    const FilePath& backing_file_path)
    : backing_file_path_(backing_file_path),
      policy_(new DictionaryValue),
      fresh_policy_(false),
      is_device_unmanaged_(false) {
}

DeviceManagementPolicyCache::~DeviceManagementPolicyCache() {}

void DeviceManagementPolicyCache::LoadPolicyFromFile() {
  if (!file_util::PathExists(backing_file_path_) || fresh_policy_)
    return;

  // Read the protobuf from the file.
  std::string data;
  if (!file_util::ReadFileToString(backing_file_path_, &data)) {
    LOG(WARNING) << "Failed to read policy data from "
                 << backing_file_path_.value();
    return;
  }

  em::CachedDevicePolicyResponse cached_policy;
  if (!cached_policy.ParseFromArray(data.c_str(), data.size())) {
    LOG(WARNING) << "Failed to parse policy data read from "
                 << backing_file_path_.value();
    return;
  }

  // Reject files that claim to be from the future.
  base::Time timestamp = base::Time::FromInternalValue(
      cached_policy.timestamp());
  if (timestamp > base::Time::NowFromSystemTime()) {
    LOG(WARNING) << "Rejected policy data from " << backing_file_path_.value()
                 << ", file is from the future.";
    return;
  }
  is_device_unmanaged_ = cached_policy.unmanaged();

  // Decode and swap in the new policy information.
  scoped_ptr<DictionaryValue> value(DecodePolicy(cached_policy.policy()));
  {
    base::AutoLock lock(lock_);
    if (!fresh_policy_)
      policy_.reset(value.release());
    last_policy_refresh_time_ = timestamp;
  }
}

bool DeviceManagementPolicyCache::SetPolicy(
    const em::DevicePolicyResponse& policy) {
  is_device_unmanaged_ = false;
  DictionaryValue* value = DeviceManagementPolicyCache::DecodePolicy(policy);
  const bool new_policy_differs = !(value->Equals(policy_.get()));
  base::Time now(base::Time::NowFromSystemTime());
  {
    base::AutoLock lock(lock_);
    policy_.reset(value);
    fresh_policy_ = true;
    last_policy_refresh_time_ = now;
  }

  em::DevicePolicyResponse* policy_copy = new em::DevicePolicyResponse;
  policy_copy->CopyFrom(policy);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      new PersistPolicyTask(backing_file_path_, policy_copy, now, false));
  return new_policy_differs;
}

DictionaryValue* DeviceManagementPolicyCache::GetPolicy() {
  base::AutoLock lock(lock_);
  return policy_->DeepCopy();
}

void DeviceManagementPolicyCache::SetDeviceUnmanaged() {
  is_device_unmanaged_ = true;
  base::Time now(base::Time::NowFromSystemTime());
  {
    base::AutoLock lock(lock_);
    policy_.reset(new DictionaryValue);
    last_policy_refresh_time_ = now;
  }
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      new PersistPolicyTask(backing_file_path_, NULL, now, true));
}

// static
Value* DeviceManagementPolicyCache::DecodeIntegerValue(
    google::protobuf::int64 value) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return NULL;
  }

  return Value::CreateIntegerValue(static_cast<int>(value));
}

// static
Value* DeviceManagementPolicyCache::DecodeValue(const em::GenericValue& value) {
  if (!value.has_value_type())
    return NULL;

  switch (value.value_type()) {
    case em::GenericValue::VALUE_TYPE_BOOL:
      if (value.has_bool_value())
        return Value::CreateBooleanValue(value.bool_value());
      return NULL;
    case em::GenericValue::VALUE_TYPE_INT64:
      if (value.has_int64_value())
        return DecodeIntegerValue(value.int64_value());
      return NULL;
    case em::GenericValue::VALUE_TYPE_STRING:
      if (value.has_string_value())
        return Value::CreateStringValue(value.string_value());
      return NULL;
    case em::GenericValue::VALUE_TYPE_DOUBLE:
      if (value.has_double_value())
        return Value::CreateDoubleValue(value.double_value());
      return NULL;
    case em::GenericValue::VALUE_TYPE_BYTES:
      if (value.has_bytes_value()) {
        std::string bytes = value.bytes_value();
        return BinaryValue::CreateWithCopiedBuffer(bytes.c_str(), bytes.size());
      }
      return NULL;
    case em::GenericValue::VALUE_TYPE_BOOL_ARRAY: {
      ListValue* list = new ListValue;
      RepeatedField<bool>::const_iterator i;
      for (i = value.bool_array().begin(); i != value.bool_array().end(); ++i)
        list->Append(Value::CreateBooleanValue(*i));
      return list;
    }
    case em::GenericValue::VALUE_TYPE_INT64_ARRAY: {
      ListValue* list = new ListValue;
      RepeatedField<google::protobuf::int64>::const_iterator i;
      for (i = value.int64_array().begin();
           i != value.int64_array().end(); ++i) {
        Value* int_value = DecodeIntegerValue(*i);
        if (int_value)
          list->Append(int_value);
      }
      return list;
    }
    case em::GenericValue::VALUE_TYPE_STRING_ARRAY: {
      ListValue* list = new ListValue;
      RepeatedPtrField<std::string>::const_iterator i;
      for (i = value.string_array().begin();
           i != value.string_array().end(); ++i)
        list->Append(Value::CreateStringValue(*i));
      return list;
    }
    case em::GenericValue::VALUE_TYPE_DOUBLE_ARRAY: {
      ListValue* list = new ListValue;
      RepeatedField<double>::const_iterator i;
      for (i = value.double_array().begin();
           i != value.double_array().end(); ++i)
        list->Append(Value::CreateDoubleValue(*i));
      return list;
    }
    default:
      NOTREACHED() << "Unhandled value type";
  }

  return NULL;
}

// static
DictionaryValue* DeviceManagementPolicyCache::DecodePolicy(
    const em::DevicePolicyResponse& policy) {
  DictionaryValue* result = new DictionaryValue;
  RepeatedPtrField<em::DevicePolicySetting>::const_iterator setting;
  for (setting = policy.setting().begin();
       setting != policy.setting().end();
       ++setting) {
    // Wrong policy key? Skip.
    if (setting->policy_key().compare(kChromeDevicePolicySettingKey) != 0)
      continue;

    // No policy value? Skip.
    if (!setting->has_policy_value())
      continue;

    // Iterate through all the name-value pairs wrapped in |setting|.
    const em::GenericSetting& policy_value(setting->policy_value());
    RepeatedPtrField<em::GenericNamedValue>::const_iterator named_value;
    for (named_value = policy_value.named_value().begin();
         named_value != policy_value.named_value().end();
         ++named_value) {
      if (named_value->has_value()) {
        Value* decoded_value =
            DeviceManagementPolicyCache::DecodeValue(named_value->value());
        if (decoded_value)
          result->Set(named_value->name(), decoded_value);
      }
    }
  }
  return result;
}

}  // namespace policy
