// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_TEST_UTIL_H_
#define EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_TEST_UTIL_H_

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/api/storage/settings_storage_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"

class ValueStore;

namespace extensions {

class StorageFrontend;
// Utilities for extension settings API tests.
namespace settings_test_util {

// Synchronously gets the storage area for an extension from |frontend|.
ValueStore* GetStorage(scoped_refptr<const Extension> extension,
                       settings_namespace::Namespace setting_namespace,
                       StorageFrontend* frontend);

// Synchronously gets the SYNC storage for an extension from |frontend|.
ValueStore* GetStorage(scoped_refptr<const Extension> extension,
                       StorageFrontend* frontend);

// Creates an extension with |id| and adds it to the registry for |profile|.
scoped_refptr<const Extension> AddExtensionWithId(Profile* profile,
                                                  const std::string& id,
                                                  Manifest::Type type);

// Creates an extension with |id| with a set of |permissions| and adds it to
// the registry for |profile|.
scoped_refptr<const Extension> AddExtensionWithIdAndPermissions(
    Profile* profile,
    const std::string& id,
    Manifest::Type type,
    const std::set<std::string>& permissions);

// A mock ExtensionSystem to serve an EventRouter.
class MockExtensionSystem : public TestExtensionSystem {
 public:
  explicit MockExtensionSystem(Profile* profile);
  virtual ~MockExtensionSystem();

  virtual EventRouter* event_router() OVERRIDE;

 private:
  scoped_ptr<EventRouter> event_router_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionSystem);
};

// A Profile which returns an ExtensionSystem with enough functionality for
// the tests.
class MockProfile : public TestingProfile {
 public:
  explicit MockProfile(const base::FilePath& file_path);
  virtual ~MockProfile();
};

// SettingsStorageFactory which acts as a wrapper for other factories.
class ScopedSettingsStorageFactory : public SettingsStorageFactory {
 public:
  ScopedSettingsStorageFactory();

  explicit ScopedSettingsStorageFactory(
      const scoped_refptr<SettingsStorageFactory>& delegate);

  // Sets the delegate factory (equivalent to scoped_ptr::reset).
  void Reset(const scoped_refptr<SettingsStorageFactory>& delegate);

  // SettingsStorageFactory implementation.
  virtual ValueStore* Create(const base::FilePath& base_path,
                             const std::string& extension_id) OVERRIDE;
  virtual void DeleteDatabaseIfExists(
      const base::FilePath& base_path,
      const std::string& extension_id) OVERRIDE;

 private:
  // SettingsStorageFactory is refcounted.
  virtual ~ScopedSettingsStorageFactory();

  scoped_refptr<SettingsStorageFactory> delegate_;
};

}  // namespace settings_test_util

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_TEST_UTIL_H_
