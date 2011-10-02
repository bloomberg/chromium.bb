// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_H_
#pragma once

#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

// Interface for extension settings storage classes.
//
// All methods *must* be run on the FILE thread, inclusing construction and
// destructions.
class ExtensionSettingsStorage {
 public:
  // The result of an operation.
  //
  // Supports lightweight copying.
  class Result {
   public:
    // Ownership of |settings| and |changed_keys| taken.
    // |settings| may be NULL when the result is for an operation which
    // generates no setting values (e.g. Remove(), Clear()).
    // |changed_keys| may be NULL when the result is for an operation which
    // cannot change settings (e.g. Get()).
    Result(DictionaryValue* settings, std::set<std::string>* changed_keys);
    explicit Result(const std::string& error);
    ~Result();

    // The dictionary result of the computation.  NULL does not imply invalid;
    // HasError() should be used to test this.
    // Ownership remains with with the Result.
    DictionaryValue* GetSettings() const;

    // Gets the list of setting keys which changed as a result of the
    // computation.  This includes all settings that existed and removed, all
    // settings which changed when set, and all setting keys cleared.
    // May be NULL for operations which cannot change settings, such as Get().
    std::set<std::string>* GetChangedKeys() const;

    // Whether there was an error in the computation.  If so, the results of
    // GetSettings and ReleaseSettings are not valid.
    bool HasError() const;

    // Gets the error message, if any.
    const std::string& GetError() const;

   private:
    struct Inner : public base::RefCountedThreadSafe<Inner> {
      // Empty error implies no error.
      Inner(
          DictionaryValue* settings,
          std::set<std::string>* changed_keys,
          const std::string& error);
      ~Inner();

      const scoped_ptr<DictionaryValue> settings_;
      const scoped_ptr<std::set<std::string> > changed_keys_;
      const std::string error_;
    };

    const scoped_refptr<Inner> inner_;
  };

  virtual ~ExtensionSettingsStorage() {}

  // Gets a single value from storage.
  // If successful, result maps the key to its value.
  virtual Result Get(const std::string& key) = 0;

  // Gets multiple values from storage.
  // If successful, result maps each key to its value.
  virtual Result Get(const std::vector<std::string>& keys) = 0;

  // Gets all values from storage.
  // If successful, result maps every key to its value.
  virtual Result Get() = 0;

  // Sets a single key to a new value.
  // If successful, result maps the key to the given value.
  virtual Result Set(const std::string& key, const Value& value) = 0;

  // Sets multiple keys to new values.
  // If successful, result is identical to the given dictionary.
  virtual Result Set(const DictionaryValue& values) = 0;

  // Removes a key from the storage.
  // If successful, result value is NULL.
  virtual Result Remove(const std::string& key) = 0;

  // Removes multiple keys from the storage.
  // If successful, result value is NULL.
  virtual Result Remove(const std::vector<std::string>& keys) = 0;

  // Clears the storage.
  // If successful, result value is NULL.
  virtual Result Clear() = 0;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_H_
