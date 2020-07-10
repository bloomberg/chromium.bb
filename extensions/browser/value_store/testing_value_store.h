// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_TESTING_VALUE_STORE_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_TESTING_VALUE_STORE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "extensions/browser/value_store/value_store.h"

// ValueStore for testing, with an in-memory storage but the ability to
// optionally fail all operations.
class TestingValueStore : public ValueStore {
 public:
  TestingValueStore();
  ~TestingValueStore() override;

  // Sets the error code for requests. If OK, errors won't be thrown.
  // Defaults to OK.
  void set_status_code(StatusCode status_code);

  // Accessors for the number of reads/writes done by this value store. Each
  // Get* operation (except for the BytesInUse ones) counts as one read, and
  // each Set*/Remove/Clear operation counts as one write. This is useful in
  // tests seeking to assert that some number of reads/writes to their
  // underlying value store have (or have not) happened.
  int read_count() { return read_count_; }
  int write_count() { return write_count_; }

  // ValueStore implementation.
  size_t GetBytesInUse(const std::string& key) override;
  size_t GetBytesInUse(const std::vector<std::string>& keys) override;
  size_t GetBytesInUse() override;
  ReadResult Get(const std::string& key) override;
  ReadResult Get(const std::vector<std::string>& keys) override;
  ReadResult Get() override;
  WriteResult Set(WriteOptions options,
                  const std::string& key,
                  const base::Value& value) override;
  WriteResult Set(WriteOptions options,
                  const base::DictionaryValue& values) override;
  WriteResult Remove(const std::string& key) override;
  WriteResult Remove(const std::vector<std::string>& keys) override;
  WriteResult Clear() override;

 private:
  base::DictionaryValue storage_;
  int read_count_;
  int write_count_;
  ValueStore::Status status_;

  DISALLOW_COPY_AND_ASSIGN(TestingValueStore);
};

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_TESTING_VALUE_STORE_H_
