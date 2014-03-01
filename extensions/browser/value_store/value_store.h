// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/browser/value_store/value_store_change.h"

// Interface for a storage area for Value objects.
class ValueStore {
 public:
  // Error codes returned from storage methods.
  enum ErrorCode {
    OK,

    // The failure was due to some kind of database corruption. Depending on
    // what is corrupted, some part of the database may be recoverable.
    //
    // For example, if the on-disk representation of leveldb is corrupted, it's
    // likely the whole database will need to be wiped and started again.
    //
    // If a single key has been committed with an invalid JSON representation,
    // just that key can be deleted without affecting the rest of the database.
    CORRUPTION,

    // The failure was due to the store being read-only (for example, policy).
    READ_ONLY,

    // The failure was due to the store running out of space.
    QUOTA_EXCEEDED,

    // Any other error.
    OTHER_ERROR,
  };

  // Bundles an ErrorCode with further metadata.
  struct Error {
    Error(ErrorCode code,
          const std::string& message,
          scoped_ptr<std::string> key);
    ~Error();

    static scoped_ptr<Error> Create(ErrorCode code,
                                    const std::string& message,
                                    scoped_ptr<std::string> key) {
      return make_scoped_ptr(new Error(code, message, key.Pass()));
    }

    // The error code.
    const ErrorCode code;

    // Message associated with the error.
    const std::string message;

    // The key associated with the error, if any. Use a scoped_ptr here
    // because empty-string is a valid key.
    //
    // TODO(kalman): add test(s) for an empty key.
    const scoped_ptr<std::string> key;

   private:
    DISALLOW_COPY_AND_ASSIGN(Error);
  };

  // The result of a read operation (Get).
  class ReadResultType {
   public:
    explicit ReadResultType(scoped_ptr<base::DictionaryValue> settings);
    explicit ReadResultType(scoped_ptr<Error> error);
    ~ReadResultType();

    bool HasError() const { return error_; }

    bool IsCorrupted() const {
      return error_.get() && error_->code == CORRUPTION;
    }

    // Gets the settings read from the storage. Note that this represents
    // the root object. If you request the value for key "foo", that value will
    // be in |settings|.|foo|.
    //
    // Must only be called if there is no error.
    base::DictionaryValue& settings() { return *settings_; }
    scoped_ptr<base::DictionaryValue> PassSettings() {
      return settings_.Pass();
    }

    // Only call if HasError is true.
    const Error& error() const { return *error_; }
    scoped_ptr<Error> PassError() { return error_.Pass(); }

   private:
    scoped_ptr<base::DictionaryValue> settings_;
    scoped_ptr<Error> error_;

    DISALLOW_COPY_AND_ASSIGN(ReadResultType);
  };
  typedef scoped_ptr<ReadResultType> ReadResult;

  // The result of a write operation (Set/Remove/Clear).
  class WriteResultType {
   public:
    explicit WriteResultType(scoped_ptr<ValueStoreChangeList> changes);
    explicit WriteResultType(scoped_ptr<Error> error);
    ~WriteResultType();

    bool HasError() const { return error_; }

    // Gets the list of changes to the settings which resulted from the write.
    // Won't be present if the NO_GENERATE_CHANGES WriteOptions was given.
    // Only call if HasError is false.
    ValueStoreChangeList& changes() { return *changes_; }
    scoped_ptr<ValueStoreChangeList> PassChanges() { return changes_.Pass(); }

    // Only call if HasError is true.
    const Error& error() const { return *error_; }
    scoped_ptr<Error> PassError() { return error_.Pass(); }

   private:
    scoped_ptr<ValueStoreChangeList> changes_;
    scoped_ptr<Error> error_;

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
  };
  typedef int WriteOptions;

  virtual ~ValueStore() {}

  // Helpers for making a Read/WriteResult.
  template<typename T>
  static ReadResult MakeReadResult(scoped_ptr<T> arg) {
    return ReadResult(new ReadResultType(arg.Pass()));
  }

  template<typename T>
  static WriteResult MakeWriteResult(scoped_ptr<T> arg) {
    return WriteResult(new WriteResultType(arg.Pass()));
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
  virtual WriteResult Set(WriteOptions options,
                          const std::string& key,
                          const base::Value& value) = 0;

  // Sets multiple keys to new values.
  virtual WriteResult Set(
      WriteOptions options, const base::DictionaryValue& values) = 0;

  // Removes a key from the storage.
  virtual WriteResult Remove(const std::string& key) = 0;

  // Removes multiple keys from the storage.
  virtual WriteResult Remove(const std::vector<std::string>& keys) = 0;

  // Clears the storage.
  virtual WriteResult Clear() = 0;

  // In the event of corruption, the ValueStore should be able to restore
  // itself. This means deleting local corrupted files. If only a few keys are
  // corrupted, then some of the database may be saved. If the full database is
  // corrupted, this will erase it in its entirety.
  // Returns true on success, false on failure. The only way this will fail is
  // if we also cannot delete the database file.
  // Note: This method may be expensive; some implementations may need to read
  // the entire database to restore. Use sparingly.
  // Note: This method (and the following RestoreKey()) are rude, and do not
  // make any logs, track changes, or other generally polite things. Please do
  // not use these as substitutes for Clear() and Remove().
  virtual bool Restore() = 0;

  // Similar to Restore(), but for only a particular key. If the key is corrupt,
  // this will forcefully remove the key. It does not look at the database on
  // the whole, which makes it faster, but does not guarantee there is no
  // additional corruption.
  // Returns true on success, and false on failure. If false, the next step is
  // probably to Restore() the whole database.
  virtual bool RestoreKey(const std::string& key) = 0;
};

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_H_
