// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_TEST_UTIL_H_

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/settings_namespace.h"
#include "chrome/browser/extensions/settings/settings_storage_factory.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/testing_profile.h"

class ValueStore;

namespace extensions {

class SettingsFrontend;
// Utilities for extension settings API tests.
namespace settings_test_util {

// Synchronously gets the storage area for an extension from |frontend|.
ValueStore* GetStorage(
    const std::string& extension_id,
    settings_namespace::Namespace setting_namespace,
    SettingsFrontend* frontend);

// Synchronously gets the SYNC storage for an extension from |frontend|.
ValueStore* GetStorage(
    const std::string& extension_id,
    SettingsFrontend* frontend);

// An ExtensionService which allows extensions to be hand-added to be returned
// by GetExtensionById.
class MockExtensionService : public TestExtensionService {
 public:
  MockExtensionService();
  virtual ~MockExtensionService();

  // Adds an extension with id |id| to be returned by GetExtensionById.
  void AddExtensionWithId(const std::string& id, Extension::Type type);

  // Adds an extension with id |id| to be returned by GetExtensionById, with
  // a set of permissions.
  void AddExtensionWithIdAndPermissions(
      const std::string& id,
      Extension::Type type,
      const std::set<std::string>& permissions);

  virtual const Extension* GetExtensionById(
      const std::string& id, bool include_disabled) const OVERRIDE;

 private:
  std::map<std::string, scoped_refptr<Extension> > extensions_;
};

// A mock ExtensionSystem to serve an EventRouter.
class MockExtensionSystem : public TestExtensionSystem {
 public:
  explicit MockExtensionSystem(Profile* profile);
  virtual ~MockExtensionSystem();

  virtual EventRouter* event_router() OVERRIDE;
  virtual ExtensionService* extension_service() OVERRIDE;

 private:
  scoped_ptr<EventRouter> event_router_;
  MockExtensionService extension_service_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionSystem);
};

// A Profile which returns an ExtensionService with enough functionality for
// the tests.
class MockProfile : public TestingProfile {
 public:
  explicit MockProfile(const FilePath& file_path);
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
  virtual ValueStore* Create(const FilePath& base_path,
                             const std::string& extension_id) OVERRIDE;

 private:
  // SettingsStorageFactory is refcounted.
  virtual ~ScopedSettingsStorageFactory();

  scoped_refptr<SettingsStorageFactory> delegate_;
};

}  // namespace settings_test_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_TEST_UTIL_H_
