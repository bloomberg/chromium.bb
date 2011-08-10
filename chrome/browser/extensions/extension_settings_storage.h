// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_H_
#pragma once

#include "base/values.h"

// Interface for extension settings storage classes.
//
// All asynchrous methods *must* run in a message loop, i.e. the callbacks may
// not be run from the calling method, but must be PostTask'ed (whether to
// one's own thread or to e.g. the FILE thread).
class ExtensionSettingsStorage {
 public:
  // Asynchronous results of Set/Get/Remove.  Exactly one of OnSuccess or
  // OnFailure will eventually be called.
  // Callback objects will be deleted after running.
  class Callback {
   public:
    virtual ~Callback() {}

    // Indicates the storage operation was successful.  Settings value will be
    // non-NULL.  Ownership is passed to the callback.
    virtual void OnSuccess(DictionaryValue* settings) = 0;

    // Indicates the storage operation failed.  Messages describes the failure.
    virtual void OnFailure(const std::string& message) = 0;
  };

  // The different types of extension settings storage.
  enum Type {
    NONE,
    NOOP,
    LEVELDB
  };

  virtual ~ExtensionSettingsStorage() {}

  // Destroys this settings storage object.  This is needed as a separate
  // interface method as opposed to just using the destructor, since
  // destruction may need to be done asynchronously (e.g. on the FILE thread).
  virtual void DeleteSoon() = 0;

  // Gets a single value from storage.  Callback with a dictionary mapping the
  // key to its value, if any.
  virtual void Get(const std::string& key, Callback* callback) = 0;

  // Gets multiple values from storage.  Callback with a dictionary mapping
  // each key to its value, if any.
  virtual void Get(const ListValue& keys, Callback* callback) = 0;

  // Gets all values from storage.  Callback with a dictionary mapping every
  // key to its value.
  virtual void Get(Callback* callback) = 0;

  // Sets a single key to a new value.  Callback with a dictionary mapping the
  // key to its new value; on success, this is guaranteed to be the given key
  // to the given new value.
  virtual void Set(const std::string& key, const Value& value,
      Callback* callback) = 0;

  // Sets multiple keys to new values. Callback with a dictionary mapping each
  // key to its new value; on success, this is guaranteed to be each given key
  // to its given new value.
  virtual void Set(const DictionaryValue& values, Callback* callback) = 0;

  // Removes a key from the map.  Callback with a dictionary mapping the key
  // to its new value; on success, this will be an empty map.
  virtual void Remove(const std::string& key, Callback* callback) = 0;

  // Removes multiple keys from the map.  Callback with a dictionary mapping
  // each key to its new value; on success, this will be an empty map.
  virtual void Remove(const ListValue& keys, Callback* callback) = 0;

  // Clears the storage.  Callback with a dictionary mapping every key in
  // storage to its value; on success, this will be an empty map.
  virtual void Clear(Callback *callback) = 0;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_H_
