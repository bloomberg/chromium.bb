// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/quota_internals/quota_internals_types.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/net_util.h"

namespace {

std::string StorageTypeToString(storage::StorageType type) {
  switch (type) {
    case storage::kStorageTypeTemporary:
      return "temporary";
    case storage::kStorageTypePersistent:
      return "persistent";
    case storage::kStorageTypeSyncable:
      return "syncable";
    case storage::kStorageTypeQuotaNotManaged:
      return "quota not managed";
    case storage::kStorageTypeUnknown:
      return "unknown";
  }
  return "unknown";
}

}  // anonymous namespace

namespace quota_internals {

GlobalStorageInfo::GlobalStorageInfo(storage::StorageType type)
    : type_(type), usage_(-1), unlimited_usage_(-1), quota_(-1) {
}

GlobalStorageInfo::~GlobalStorageInfo() {}

base::Value* GlobalStorageInfo::NewValue() const {
  // TODO(tzik): Add CreateLongIntegerValue to base/values.h and remove
  // all static_cast<double> in this file.
  base::DictionaryValue* dict = new base::DictionaryValue;
  dict->SetString("type", StorageTypeToString(type_));
  if (usage_ >= 0)
    dict->SetDouble("usage", static_cast<double>(usage_));
  if (unlimited_usage_ >= 0)
    dict->SetDouble("unlimitedUsage", static_cast<double>(unlimited_usage_));
  if (quota_ >= 0)
    dict->SetDouble("quota", static_cast<double>(quota_));
  return dict;
}

PerHostStorageInfo::PerHostStorageInfo(const std::string& host,
                                       storage::StorageType type)
    : host_(host), type_(type), usage_(-1), quota_(-1) {
}

PerHostStorageInfo::~PerHostStorageInfo() {}

base::Value* PerHostStorageInfo::NewValue() const {
  base::DictionaryValue* dict = new base::DictionaryValue;
  DCHECK(!host_.empty());
  dict->SetString("host", host_);
  dict->SetString("type", StorageTypeToString(type_));
  if (usage_ >= 0)
    dict->SetDouble("usage", static_cast<double>(usage_));
  if (quota_ >= 0)
    dict->SetDouble("quota", static_cast<double>(quota_));
  return dict;
}

PerOriginStorageInfo::PerOriginStorageInfo(const GURL& origin,
                                           storage::StorageType type)
    : origin_(origin),
      type_(type),
      host_(net::GetHostOrSpecFromURL(origin)),
      in_use_(-1),
      used_count_(-1) {
}

PerOriginStorageInfo::~PerOriginStorageInfo() {}

base::Value* PerOriginStorageInfo::NewValue() const {
  base::DictionaryValue* dict = new base::DictionaryValue;
  DCHECK(!origin_.is_empty());
  DCHECK(!host_.empty());
  dict->SetString("origin", origin_.spec());
  dict->SetString("type", StorageTypeToString(type_));
  dict->SetString("host", host_);
  if (in_use_ >= 0)
    dict->SetBoolean("inUse", (in_use_ > 0));
  if (used_count_ >= 0)
    dict->SetInteger("usedCount", used_count_);
  if (!last_access_time_.is_null())
    dict->SetDouble("lastAccessTime", last_access_time_.ToDoubleT() * 1000.0);
  if (!last_modified_time_.is_null()) {
    dict->SetDouble("lastModifiedTime",
                    last_modified_time_.ToDoubleT() * 1000.0);
  }
  return dict;
}

}  // namespace quota_internals
