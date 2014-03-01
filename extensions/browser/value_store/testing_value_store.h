// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_TESTING_VALUE_STORE_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_TESTING_VALUE_STORE_H_

#include "base/compiler_specific.h"
#include "extensions/browser/value_store/value_store.h"

// ValueStore for testing, with an in-memory storage but the ability to
// optionally fail all operations.
class TestingValueStore : public ValueStore {
 public:
  TestingValueStore();
  virtual ~TestingValueStore();

  // Sets the error code for requests. If OK, errors won't be thrown.
  // Defaults to OK.
  void set_error_code(ErrorCode error_code) { error_code_ = error_code; }

  // Accessors for the number of reads/writes done by this value store. Each
  // Get* operation (except for the BytesInUse ones) counts as one read, and
  // each Set*/Remove/Clear operation counts as one write. This is useful in
  // tests seeking to assert that some number of reads/writes to their
  // underlying value store have (or have not) happened.
  int read_count() { return read_count_; }
  int write_count() { return write_count_; }

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
  // TestingValueStores can't get corrupted (they're all in-memory), so these
  // just return true.
  virtual bool Restore() OVERRIDE;
  virtual bool RestoreKey(const std::string& key) OVERRIDE;

 private:
  scoped_ptr<ValueStore::Error> TestingError();

  base::DictionaryValue storage_;
  int read_count_;
  int write_count_;
  ErrorCode error_code_;

  DISALLOW_COPY_AND_ASSIGN(TestingValueStore);
};

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_TESTING_VALUE_STORE_H_
