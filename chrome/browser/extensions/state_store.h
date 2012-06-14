// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_STATE_STORE_H_
#define CHROME_BROWSER_EXTENSIONS_STATE_STORE_H_
#pragma once

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/value_store/value_store_frontend.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {

// A storage area for per-extension state that needs to be persisted to disk.
class StateStore
    : public base::SupportsWeakPtr<StateStore>,
      public content::NotificationObserver {
 public:
  typedef ValueStoreFrontend::ReadCallback ReadCallback;

  StateStore(Profile* profile, const FilePath& db_path);
  // This variant is useful for testing (using a mock ValueStore).
  StateStore(Profile* profile, ValueStore* store);
  virtual ~StateStore();

  // Register a key for removal upon extension install/uninstall. We remove
  // for install to reset state when an extension upgrades.
  void RegisterKey(const std::string& key);

  // Get the value associated with the given extension and key, and pass
  // it to |callback| asynchronously.
  void GetExtensionValue(const std::string& extension_id,
                         const std::string& key,
                         ReadCallback callback);

  // Sets a value for a given extension and key.
  void SetExtensionValue(const std::string& extension_id,
                         const std::string& key,
                         scoped_ptr<base::Value> value);

 private:
  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // The store that holds our key/values.
  ValueStoreFrontend store_;

  // List of all known keys. They will be cleared for each extension when it is
  // (un)installed.
  std::set<std::string> registered_keys_;

  content::NotificationRegistrar registrar_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_STATE_STORE_H_
