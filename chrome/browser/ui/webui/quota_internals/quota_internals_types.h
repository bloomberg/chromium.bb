// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_QUOTA_INTERNALS_TYPES_H_
#define CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_QUOTA_INTERNALS_TYPES_H_

#include <map>
#include <string>

#include "base/time/time.h"
#include "url/gurl.h"
#include "webkit/common/quota/quota_types.h"

namespace base {
class Value;
}

namespace quota_internals {

// Represends global usage and quota information for specific type of storage.
class GlobalStorageInfo {
 public:
  explicit GlobalStorageInfo(quota::StorageType type);
  ~GlobalStorageInfo();

  void set_usage(int64 usage) {
    usage_ = usage;
  }

  void set_unlimited_usage(int64 unlimited_usage) {
    unlimited_usage_ = unlimited_usage;
  }

  void set_quota(int64 quota) {
    quota_ = quota;
  }

  // Create new Value for passing to WebUI page.  Caller is responsible for
  // deleting the returned pointer.
  base::Value* NewValue() const;
 private:
  quota::StorageType type_;

  int64 usage_;
  int64 unlimited_usage_;
  int64 quota_;
};

// Represents per host usage and quota information for the storage.
class PerHostStorageInfo {
 public:
  PerHostStorageInfo(const std::string& host, quota::StorageType type);
  ~PerHostStorageInfo();

  void set_usage(int64 usage) {
    usage_ = usage;
  }

  void set_quota(int64 quota) {
    quota_ = quota;
  }

  // Create new Value for passing to WebUI page.  Caller is responsible for
  // deleting the returned pointer.
  base::Value* NewValue() const;
 private:
  std::string host_;
  quota::StorageType type_;

  int64 usage_;
  int64 quota_;
};

// Represendts per origin usage and access time information.
class PerOriginStorageInfo {
 public:
  PerOriginStorageInfo(const GURL& origin, quota::StorageType type);
  ~PerOriginStorageInfo();

  void set_in_use(bool in_use) {
    in_use_ = in_use ? 1 : 0;
  }

  void set_used_count(int used_count) {
    used_count_ = used_count;
  }

  void set_last_access_time(base::Time last_access_time) {
    last_access_time_ = last_access_time;
  }

  void set_last_modified_time(base::Time last_modified_time) {
    last_modified_time_ = last_modified_time;
  }

  // Create new Value for passing to WebUI page.  Caller is responsible for
  // deleting the returned pointer.
  base::Value* NewValue() const;
 private:
  GURL origin_;
  quota::StorageType type_;
  std::string host_;

  int in_use_;
  int used_count_;
  base::Time last_access_time_;
  base::Time last_modified_time_;
};
}  // quota_internals

#endif  // CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_QUOTA_INTERNALS_TYPES_H_
