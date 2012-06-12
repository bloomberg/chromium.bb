// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VALUE_STORE_TESTING_VALUE_STORE_H_
#define CHROME_BROWSER_VALUE_STORE_TESTING_VALUE_STORE_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/value_store/value_store.h"

// ValueStore for testing, with an in-memory storage but the ability to
// optionally fail all operations.
class TestingValueStore : public ValueStore {
 public:
  TestingValueStore();
  virtual ~TestingValueStore();

  // Sets whether to fail all requests (default is false).
  void SetFailAllRequests(bool fail_all_requests);

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
  DictionaryValue storage_;

  bool fail_all_requests_;

  DISALLOW_COPY_AND_ASSIGN(TestingValueStore);
};

#endif  // CHROME_BROWSER_VALUE_STORE_TESTING_VALUE_STORE_H_
