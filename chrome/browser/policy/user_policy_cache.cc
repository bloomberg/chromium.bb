// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_cache.h"

#include <limits>
#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chrome/browser/policy/proto/old_generic_format.pb.h"
#include "content/browser/browser_thread.h"
#include "policy/configuration_policy_type.h"

namespace policy {

// Decodes a CloudPolicySettings object into two maps with mandatory and
// recommended settings, respectively. The implementation is generated code
// in policy/cloud_policy_generated.cc.
void DecodePolicy(const em::CloudPolicySettings& policy,
                  PolicyMap* mandatory, PolicyMap* recommended);

// Handles the on-disk cache file used by UserPolicyCache. This class handles
// the necessary thread switching and may outlive the associated UserPolicyCache
// instance.
class UserPolicyCache::DiskCache
    : public base::RefCountedThreadSafe<UserPolicyCache::DiskCache> {
 public:
  DiskCache(const base::WeakPtr<UserPolicyCache>& cache,
            const FilePath& backing_file_path);

  // Starts reading the policy cache from disk. Passes the read policy
  // information back to the hosting UserPolicyCache after a successful cache
  // load through UserPolicyCache::OnDiskCacheLoaded().
  void Load();

  // Triggers a write operation to the disk cache on the FILE thread.
  void Store(const em::CachedCloudPolicyResponse& policy);

 private:
  // Tries to load the cache file on the FILE thread.
  void LoadOnFileThread();

  // Passes back the successfully read policy to the cache on the UI thread.
  void FinishLoadOnUIThread(const em::CachedCloudPolicyResponse& policy);

  // Saves a policy blob on the FILE thread.
  void StoreOnFileThread(const em::CachedCloudPolicyResponse& policy);

  base::WeakPtr<UserPolicyCache> cache_;
  const FilePath backing_file_path_;

  DISALLOW_COPY_AND_ASSIGN(DiskCache);
};

UserPolicyCache::DiskCache::DiskCache(
    const base::WeakPtr<UserPolicyCache>& cache,
    const FilePath& backing_file_path)
    : cache_(cache),
      backing_file_path_(backing_file_path) {}

void UserPolicyCache::DiskCache::Load() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &DiskCache::LoadOnFileThread));
}

void UserPolicyCache::DiskCache::Store(
    const em::CachedCloudPolicyResponse& policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &DiskCache::StoreOnFileThread, policy));
}

void UserPolicyCache::DiskCache::LoadOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!file_util::PathExists(backing_file_path_))
    return;

  // Read the protobuf from the file.
  std::string data;
  if (!file_util::ReadFileToString(backing_file_path_, &data)) {
    LOG(WARNING) << "Failed to read policy data from "
                 << backing_file_path_.value();
    return;
  }

  // Decode it.
  em::CachedCloudPolicyResponse cached_response;
  if (!cached_response.ParseFromArray(data.c_str(), data.size())) {
    LOG(WARNING) << "Failed to parse policy data read from "
                 << backing_file_path_.value();
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &DiskCache::FinishLoadOnUIThread,
                        cached_response));
}

void UserPolicyCache::DiskCache::FinishLoadOnUIThread(
    const em::CachedCloudPolicyResponse& policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (cache_.get())
    cache_->OnDiskCacheLoaded(policy);
}


void UserPolicyCache::DiskCache::StoreOnFileThread(
    const em::CachedCloudPolicyResponse& policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::string data;
  if (!policy.SerializeToString(&data)) {
    LOG(WARNING) << "Failed to serialize policy data";
    return;
  }

  if (!file_util::CreateDirectory(backing_file_path_.DirName())) {
    LOG(WARNING) << "Failed to create directory "
                 << backing_file_path_.DirName().value();
    return;
  }

  int size = data.size();
  if (file_util::WriteFile(backing_file_path_, data.c_str(), size) != size) {
    LOG(WARNING) << "Failed to write " << backing_file_path_.value();
    return;
  }
}

UserPolicyCache::UserPolicyCache(const FilePath& backing_file_path)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  disk_cache_ = new DiskCache(weak_ptr_factory_.GetWeakPtr(),
                              backing_file_path);
}

UserPolicyCache::~UserPolicyCache() {
}

void UserPolicyCache::Load() {
  disk_cache_->Load();
}

void UserPolicyCache::SetPolicy(const em::PolicyFetchResponse& policy) {
  base::Time now = base::Time::NowFromSystemTime();
  set_last_policy_refresh_time(now);
  base::Time timestamp;
  if (!SetPolicyInternal(policy, &timestamp, false))
    return;

  if (timestamp > base::Time::NowFromSystemTime() +
                  base::TimeDelta::FromMinutes(1)) {
    LOG(WARNING) << "Server returned policy with timestamp from the future, "
                    "not persisting to disk.";
    return;
  }

  em::CachedCloudPolicyResponse cached_policy;
  cached_policy.mutable_cloud_policy()->CopyFrom(policy);
  disk_cache_->Store(cached_policy);
}

void UserPolicyCache::SetUnmanaged() {
  DCHECK(CalledOnValidThread());
  SetUnmanagedInternal(base::Time::NowFromSystemTime());

  em::CachedCloudPolicyResponse cached_policy;
  cached_policy.set_unmanaged(true);
  cached_policy.set_timestamp(base::Time::NowFromSystemTime().ToTimeT());
  disk_cache_->Store(cached_policy);
}

void UserPolicyCache::OnDiskCacheLoaded(
    const em::CachedCloudPolicyResponse& cached_response) {
  if (initialization_complete())
    return;

  if (cached_response.unmanaged()) {
    SetUnmanagedInternal(base::Time::FromTimeT(cached_response.timestamp()));
  } else if (cached_response.has_cloud_policy()) {
    base::Time timestamp;
    if (SetPolicyInternal(cached_response.cloud_policy(), &timestamp, true))
      set_last_policy_refresh_time(timestamp);
  }
}

bool UserPolicyCache::DecodePolicyData(const em::PolicyData& policy_data,
                                       PolicyMap* mandatory,
                                       PolicyMap* recommended) {
  // TODO(jkummerow): Verify policy_data.device_token(). Needs final
  // specification which token we're actually sending / expecting to get back.
  em::CloudPolicySettings policy;
  if (!policy.ParseFromString(policy_data.policy_value())) {
    LOG(WARNING) << "Failed to parse CloudPolicySettings protobuf.";
    return false;
  }
  DecodePolicy(policy, mandatory, recommended);
  MaybeDecodeOldstylePolicy(policy_data.policy_value(), mandatory, recommended);
  return true;
}

// Everything below is only needed for supporting old-style GenericNamedValue
// based policy data and can be removed once this support is no longer needed.

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

class PolicyMapProxy : public ConfigurationPolicyStoreInterface {
 public:
  // Does not take ownership of |policy_map|, and callers need to make sure
  // that |policy_map| outlives this PolicyMapProxy.
  explicit PolicyMapProxy(PolicyMap* policy_map)
      : policy_map_(policy_map) {}
  virtual ~PolicyMapProxy() {}
  virtual void Apply(ConfigurationPolicyType policy, Value* value) {
    policy_map_->Set(policy, value);
  }

 private:
  PolicyMap* policy_map_;

  DISALLOW_COPY_AND_ASSIGN(PolicyMapProxy);
};

void UserPolicyCache::MaybeDecodeOldstylePolicy(
    const std::string& policy_data,
    PolicyMap* mandatory,
    PolicyMap* recommended) {
  // Return immediately if we already have policy information in the maps.
  if (!mandatory->empty() || !recommended->empty())
    return;
  em::LegacyChromeSettingsProto policy;
  // Return if the input string doesn't match the protobuf definition.
  if (!policy.ParseFromString(policy_data))
    return;
  // Return if there's no old-style policy to decode.
  if (policy.named_value_size() == 0)
    return;

  // Inspect GenericNamedValues and decode them.
  DictionaryValue result;
  RepeatedPtrField<em::GenericNamedValue>::const_iterator named_value;
  for (named_value = policy.named_value().begin();
       named_value != policy.named_value().end();
       ++named_value) {
    if (named_value->has_value()) {
      Value* decoded_value = DecodeValue(named_value->value());
      if (decoded_value)
        result.Set(named_value->name(), decoded_value);
    }
  }
  // Hack: Let one of the providers do the transformation from DictionaryValue
  // to PolicyMap, since they have the required code anyway.
  PolicyMapProxy map_proxy(mandatory);
  GetManagedPolicyProvider()->ApplyPolicyValueTree(&result, &map_proxy);
}

Value* UserPolicyCache::DecodeIntegerValue(
    google::protobuf::int64 value) const {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return NULL;
  }

  return Value::CreateIntegerValue(static_cast<int>(value));
}

Value* UserPolicyCache::DecodeValue(const em::GenericValue& value) const {
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

}  // namespace policy
