// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_VALUE_STORE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_VALUE_STORE_CACHE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"

class ValueStore;

namespace extensions {

class Extension;

// Each namespace of the storage API implements this interface.
// Instances are created on the UI thread, but from then on live on the loop
// returned by GetMessageLoop() and every methods (except GetMessageLoop())
// are always called in that loop, including the destructor.
class ValueStoreCache {
 public:
  typedef base::Callback<void(ValueStore*)> StorageCallback;

  virtual ~ValueStoreCache();

  // Returns the loop that the methods of this class should be invoked on.
  virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoop() const = 0;

  // Requests the cache to invoke |callback| with the appropriate ValueStore
  // for the given |extension|. |callback| should be invoked with a NULL
  // ValueStore in case of errors.
  // |extension| is passed in a scoped_refptr<> because this method is
  // asynchronously posted as a task to the loop returned by GetMessageLoop(),
  // and this guarantees the Extension is still valid when the method executes.
  virtual void RunWithValueStoreForExtension(
      const StorageCallback& callback,
      scoped_refptr<const Extension> extension) = 0;

  // Requests the cache to delete any storage used by |extension_id|.
  virtual void DeleteStorageSoon(const std::string& extension_id) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_VALUE_STORE_CACHE_H_
