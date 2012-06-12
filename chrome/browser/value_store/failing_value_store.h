// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VALUE_STORE_FAILING_VALUE_STORE_H_
#define CHROME_BROWSER_VALUE_STORE_FAILING_VALUE_STORE_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/value_store/value_store.h"

// Settings storage area which fails every request.
class FailingValueStore : public ValueStore {
 public:
  FailingValueStore() {}

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
      const Value& value) OVERRIDE;
  virtual WriteResult Set(
      WriteOptions options, const DictionaryValue& values) OVERRIDE;
  virtual WriteResult Remove(const std::string& key) OVERRIDE;
  virtual WriteResult Remove(const std::vector<std::string>& keys) OVERRIDE;
  virtual WriteResult Clear() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FailingValueStore);
};

#endif  // CHROME_BROWSER_VALUE_STORE_FAILING_VALUE_STORE_H_
