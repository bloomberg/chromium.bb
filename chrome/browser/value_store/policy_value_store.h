// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VALUE_STORE_POLICY_VALUE_STORE_H_
#define CHROME_BROWSER_VALUE_STORE_POLICY_VALUE_STORE_H_

#include "base/compiler_specific.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/value_store/value_store.h"

// A value store that pulls values from the given PolicyMap. Thie is a read-only
// ValueStore, and all the write operations fail.
class PolicyValueStore : public ValueStore {
 public:
  // The values provided by this value store are pulled from the given
  // |policy_map|, which must outlive this object.
  explicit PolicyValueStore(const policy::PolicyMap* policy_map);
  virtual ~PolicyValueStore();

  // ValueStore implementation.
  virtual size_t GetBytesInUse(const std::string& key) OVERRIDE;
  virtual size_t GetBytesInUse(const std::vector<std::string>& keys) OVERRIDE;
  virtual size_t GetBytesInUse() OVERRIDE;
  virtual ReadResult Get(const std::string& key) OVERRIDE;
  virtual ReadResult Get(const std::vector<std::string>& keys) OVERRIDE;
  virtual ReadResult Get() OVERRIDE;
  virtual WriteResult Set(
      WriteOptions options,
      const std::string& key,
      const base::Value& value) OVERRIDE;
  virtual WriteResult Set(
      WriteOptions options, const base::DictionaryValue& values) OVERRIDE;
  virtual WriteResult Remove(const std::string& key) OVERRIDE;
  virtual WriteResult Remove(const std::vector<std::string>& keys) OVERRIDE;
  virtual WriteResult Clear() OVERRIDE;

 private:
  void AddEntryIfMandatory(base::DictionaryValue* result,
                           const std::string& key,
                           const policy::PolicyMap::Entry* entry);

  const policy::PolicyMap* policy_map_;

  DISALLOW_COPY_AND_ASSIGN(PolicyValueStore);
};

#endif  // CHROME_BROWSER_VALUE_STORE_POLICY_VALUE_STORE_H_
