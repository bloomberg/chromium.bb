// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_FRONTEND_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_FRONTEND_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "extensions/browser/value_store/value_store.h"

namespace base {
class FilePath;
}

// A frontend for a LeveldbValueStore, for use on the UI thread.
class ValueStoreFrontend
    : public base::SupportsWeakPtr<ValueStoreFrontend>,
      public base::NonThreadSafe {
 public:
  typedef base::Callback<void(scoped_ptr<base::Value>)> ReadCallback;

  ValueStoreFrontend();
  explicit ValueStoreFrontend(const base::FilePath& db_path);
  // This variant is useful for testing (using a mock ValueStore).
  explicit ValueStoreFrontend(scoped_ptr<ValueStore> value_store);
  ~ValueStoreFrontend();

  void Init(const base::FilePath& db_path);

  // Retrieves a value from the database asynchronously, passing a copy to
  // |callback| when ready. NULL is passed if no matching entry is found.
  void Get(const std::string& key, const ReadCallback& callback);

  // Sets a value with the given key.
  void Set(const std::string& key, scoped_ptr<base::Value> value);

  // Removes the value with the given key.
  void Remove(const std::string& key);

 private:
  class Backend;

  // A helper class to manage lifetime of the backing ValueStore, which lives
  // on the FILE thread.
  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(ValueStoreFrontend);
};

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_FRONTEND_H_
