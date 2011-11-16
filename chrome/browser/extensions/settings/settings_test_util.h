// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_TEST_UTIL_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/settings_storage_factory.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/testing_profile.h"


namespace extensions {

class SettingsFrontend;
class SettingsStorage;

// Utilities for extension settings API tests.
namespace settings_test_util {

// Synchronously gets the storage area for an extension from |frontend|.
SettingsStorage* GetStorage(
    const std::string& extension_id, SettingsFrontend* frontend);

// An ExtensionService which allows extensions to be hand-added to be returned
// by GetExtensionById.
class MockExtensionService : public TestExtensionService {
 public:
  MockExtensionService();
  virtual ~MockExtensionService();

  // Adds an extension with id |id| to be returned by GetExtensionById.
  void AddExtension(const std::string& id, Extension::Type type);

  virtual const Extension* GetExtensionById(
      const std::string& id, bool include_disabled) const OVERRIDE;

 private:
  std::map<std::string, scoped_refptr<Extension> > extensions_;
};

// A Profile which returns ExtensionService and ExtensionEventRouters with
// enough functionality for the tests.
class MockProfile : public TestingProfile {
 public:
  explicit MockProfile(const FilePath& file_path);
  virtual ~MockProfile();

  // Returns the same object as GetExtensionService, but not coaxed into an
  // ExtensionService; use this method from tests.
  MockExtensionService* GetMockExtensionService();

  virtual ExtensionService* GetExtensionService() OVERRIDE;
  virtual ExtensionEventRouter* GetExtensionEventRouter() OVERRIDE;

 private:
  MockExtensionService extension_service_;
  scoped_ptr<ExtensionEventRouter> event_router_;
};

// SettingsStorageFactory which acts as a wrapper for other factories.
class ScopedSettingsStorageFactory : public SettingsStorageFactory {
 public:
  explicit ScopedSettingsStorageFactory(SettingsStorageFactory* delegate);

  virtual ~ScopedSettingsStorageFactory();

  // Sets the delegate factory (equivalent to scoped_ptr::reset).
  void Reset(SettingsStorageFactory* delegate);

  // SettingsStorageFactory implementation.
  virtual SettingsStorage* Create(
      const FilePath& base_path, const std::string& extension_id) OVERRIDE;

 private:
  scoped_ptr<SettingsStorageFactory> delegate_;
};

}  // namespace settings_test_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_TEST_UTIL_H_
