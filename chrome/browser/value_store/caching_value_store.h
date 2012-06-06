// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VALUE_STORE_CACHING_VALUE_STORE_H_
#define CHROME_BROWSER_VALUE_STORE_CACHING_VALUE_STORE_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"

// Value store area with caching, backed by a LeveldbValueStore on the FILE
// thread. The key/value store is flat with no path expansion, meaning "foo"
// and "foo.bar" are completely separate keys.
class CachingValueStore
    : public base::SupportsWeakPtr<CachingValueStore>,
      public base::NonThreadSafe {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Notification that the Store is ready to use.
    virtual void OnInitializationComplete() = 0;
  };

  explicit CachingValueStore(const FilePath& db_path);
  ~CachingValueStore();

  // Retrieves a value from the cache, returning true if value with the given
  // key exists. The returned value is a reference to the value in the cache,
  // i.e. no copies are made.
  bool Get(const std::string& key, const base::Value** result);

  // Sets a value with the given key. Ownership of |value| is transferred to
  // the store.
  void Set(const std::string& key, base::Value* value);

  // Removes the value with the given key.
  void Remove(const std::string& key);

  // Returns true if the store has finished initializing.
  bool IsInitialized();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  class Backend;

  // Called when our backend finishes reading the database.
  void OnBackendReady(scoped_ptr<base::DictionaryValue> values);

  // A cache of the value store. This is always up-to-date, with changes
  // persisted to disk as they are made.
  scoped_ptr<base::DictionaryValue> cache_;

  // A helper class to manage lifetime of the backing ValueStore, which lives
  // on the FILE thread.
  scoped_refptr<Backend> backend_;

  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(CachingValueStore);
};

#endif  // CHROME_BROWSER_VALUE_STORE_CACHING_VALUE_STORE_H_
