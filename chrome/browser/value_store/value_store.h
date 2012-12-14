// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VALUE_STORE_VALUE_STORE_H_
#define CHROME_BROWSER_VALUE_STORE_VALUE_STORE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/value_store/value_store_change.h"

// Interface for a storage area for Value objects.
class ValueStore {
 public:
  // The result of a read operation (Get).
  class ReadResultType {
   public:
    // Ownership of |settings| taken.
    explicit ReadResultType(DictionaryValue* settings);
    explicit ReadResultType(const std::string& error);
    ~ReadResultType();

    // Gets the settings read from the storage. Note that this represents
    // the root object. If you request the value for key "foo", that value will
    // be in |settings.foo|.
    // Must only be called if HasError() is false.
    scoped_ptr<DictionaryValue>& settings();

    // Gets whether the operation failed.
    bool HasError() const;

    // Gets the error message describing the failure.
    // Must only be called if HasError() is true.
    const std::string& error() const;

   private:
    scoped_ptr<DictionaryValue> settings_;
    const std::string error_;

    DISALLOW_COPY_AND_ASSIGN(ReadResultType);
  };
  typedef scoped_ptr<ReadResultType> ReadResult;

  // The result of a write operation (Set/Remove/Clear).
  class WriteResultType {
   public:
    // Ownership of |changes| taken.
    explicit WriteResultType(ValueStoreChangeList* changes);
    explicit WriteResultType(const std::string& error);
    ~WriteResultType();

    // Gets the list of changes to the settings which resulted from the write.
    // Must only be called if HasError() is false.
    const ValueStoreChangeList& changes() const;

    // Gets whether the operation failed.
    bool HasError() const;

    // Gets the error message describing the failure.
    // Must only be called if HasError() is true.
    const std::string& error() const;

   private:
    const scoped_ptr<ValueStoreChangeList> changes_;
    const std::string error_;

    DISALLOW_COPY_AND_ASSIGN(WriteResultType);
  };
  typedef scoped_ptr<WriteResultType> WriteResult;

  // Options for write operations.
  enum WriteOptionsValues {
    // Callers should usually use this.
    DEFAULTS = 0,

    // Ignore any quota restrictions.
    IGNORE_QUOTA = 1<<1,

    // Don't generate the changes for a WriteResult.
    NO_GENERATE_CHANGES = 1<<2,

    // Don't check the old value before writing a new value. This will also
    // result in an empty |old_value| in the WriteResult::changes list.
    NO_CHECK_OLD_VALUE = 1<<3
  };
  typedef int WriteOptions;

  virtual ~ValueStore() {}

  // Helpers for making a Read/WriteResult.
  template<typename T>
  static ReadResult MakeReadResult(T arg) {
    return ReadResult(new ReadResultType(arg));
  }

  template<typename T>
  static WriteResult MakeWriteResult(T arg) {
    return WriteResult(new WriteResultType(arg));
  }

  // Gets the amount of space being used by a single value, in bytes.
  // Note: The GetBytesInUse methods are only used by extension settings at the
  // moment. If these become more generally useful, the
  // SettingsStorageQuotaEnforcer and WeakUnlimitedSettingsStorage classes
  // should be moved to the value_store directory.
  virtual size_t GetBytesInUse(const std::string& key) = 0;

  // Gets the total amount of space being used by multiple values, in bytes.
  virtual size_t GetBytesInUse(const std::vector<std::string>& keys) = 0;

  // Gets the total amount of space being used by this storage area, in bytes.
  virtual size_t GetBytesInUse() = 0;

  // Gets a single value from storage.
  virtual ReadResult Get(const std::string& key) = 0;

  // Gets multiple values from storage.
  virtual ReadResult Get(const std::vector<std::string>& keys) = 0;

  // Gets all values from storage.
  virtual ReadResult Get() = 0;

  // Sets a single key to a new value.
  virtual WriteResult Set(
      WriteOptions options, const std::string& key, const Value& value) = 0;

  // Sets multiple keys to new values.
  virtual WriteResult Set(
      WriteOptions options, const DictionaryValue& values) = 0;

  // Removes a key from the storage.
  virtual WriteResult Remove(const std::string& key) = 0;

  // Removes multiple keys from the storage.
  virtual WriteResult Remove(const std::vector<std::string>& keys) = 0;

  // Clears the storage.
  virtual WriteResult Clear() = 0;
};

#endif  // CHROME_BROWSER_VALUE_STORE_VALUE_STORE_H_
