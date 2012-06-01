// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VALUE_STORE_VALUE_STORE_H_
#define CHROME_BROWSER_VALUE_STORE_VALUE_STORE_H_
#pragma once

#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/value_store/value_store_change.h"

// Interface for a storage area for Value objects.

// All methods *must* be run on the FILE thread, including construction and
// destruction.
class ValueStore {
 public:
  // The result of a read operation (Get). Safe/efficient to copy.
  class ReadResult {
   public:
    // Ownership of |settings| taken.
    explicit ReadResult(DictionaryValue* settings);
    explicit ReadResult(const std::string& error);
    ~ReadResult();

    // Gets the settings read from the storage. Note that this represents
    // the root object. If you request the value for key "foo", that value will
    // be in |settings.foo|.
    // Must only be called if HasError() is false.
    const DictionaryValue& settings() const;

    // Gets whether the operation failed.
    bool HasError() const;

    // Gets the error message describing the failure.
    // Must only be called if HasError() is true.
    const std::string& error() const;

   private:
    class Inner : public base::RefCountedThreadSafe<Inner> {
     public:
      Inner(DictionaryValue* settings, const std::string& error);
      const scoped_ptr<DictionaryValue> settings_;
      const std::string error_;

     private:
      friend class base::RefCountedThreadSafe<Inner>;
      virtual ~Inner();
    };

    scoped_refptr<Inner> inner_;
  };

  // The result of a write operation (Set/Remove/Clear). Safe/efficient to copy.
  class WriteResult {
   public:
    // Ownership of |changes| taken.
    explicit WriteResult(ValueStoreChangeList* changes);
    explicit WriteResult(const std::string& error);
    ~WriteResult();

    // Gets the list of changes to the settings which resulted from the write.
    // Must only be called if HasError() is false.
    const ValueStoreChangeList& changes() const;

    // Gets whether the operation failed.
    bool HasError() const;

    // Gets the error message describing the failure.
    // Must only be called if HasError() is true.
    const std::string& error() const;

   private:
    class Inner : public base::RefCountedThreadSafe<Inner> {
     public:
      Inner(ValueStoreChangeList* changes, const std::string& error);
      const scoped_ptr<ValueStoreChangeList> changes_;
      const std::string error_;

     private:
      friend class base::RefCountedThreadSafe<Inner>;
      virtual ~Inner();
    };

    scoped_refptr<Inner> inner_;
  };

  // Options for write operations.
  enum WriteOptions {
    // Callers should usually use this.
    DEFAULTS,

    // Ignore any quota restrictions.
    IGNORE_QUOTA,
  };

  virtual ~ValueStore() {}

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
