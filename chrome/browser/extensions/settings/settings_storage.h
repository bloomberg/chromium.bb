// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_H_
#pragma once

#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/settings/setting_change.h"

// Interface for extension settings storage classes.
//
namespace extensions {

// All methods *must* be run on the FILE thread, including construction and
// destruction.
class SettingsStorage {
 public:
  // The result of a read operation (Get). Safe/efficient to copy.
  class ReadResult {
   public:
    // Ownership of |settings| taken.
    explicit ReadResult(DictionaryValue* settings);
    explicit ReadResult(const std::string& error);
    ~ReadResult();

    // Gets the settings read from the storage.
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
    explicit WriteResult(SettingChangeList* changes);
    explicit WriteResult(const std::string& error);
    ~WriteResult();

    // Gets the list of changes to the settings which resulted from the write.
    // Must only be called if HasError() is false.
    const SettingChangeList& changes() const;

    // Gets whether the operation failed.
    bool HasError() const;

    // Gets the error message describing the failure.
    // Must only be called if HasError() is true.
    const std::string& error() const;

   private:
    class Inner : public base::RefCountedThreadSafe<Inner> {
     public:
      Inner(SettingChangeList* changes, const std::string& error);
      const scoped_ptr<SettingChangeList> changes_;
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

    // Ignore restrictions, such as quota.  It is still possible for the
    // operation to fail, such as on hard drive failure or if the storage area
    // is configured to fail.
    FORCE
  };

  virtual ~SettingsStorage() {}

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

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_H_
