// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/settings_test_util.h"

#include "base/file_path.h"
#include "chrome/browser/extensions/api/storage/settings_frontend.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/common/extensions/extension.h"

namespace extensions {

namespace settings_test_util {

// Intended as a StorageCallback from GetStorage.
static void AssignStorage(ValueStore** dst, ValueStore* src) {
  *dst = src;
}

ValueStore* GetStorage(
    const std::string& extension_id,
    settings_namespace::Namespace settings_namespace,
    SettingsFrontend* frontend) {
  ValueStore* storage = NULL;
  frontend->RunWithStorage(
      extension_id,
      settings_namespace,
      base::Bind(&AssignStorage, &storage));
  MessageLoop::current()->RunUntilIdle();
  return storage;
}

ValueStore* GetStorage(
    const std::string& extension_id, SettingsFrontend* frontend) {
  return GetStorage(extension_id, settings_namespace::SYNC, frontend);
}

// MockExtensionService

MockExtensionService::MockExtensionService() {}

MockExtensionService::~MockExtensionService() {}

const Extension* MockExtensionService::GetExtensionById(
    const std::string& id, bool include_disabled) const {
  std::map<std::string, scoped_refptr<Extension> >::const_iterator
      maybe_extension = extensions_.find(id);
  return maybe_extension == extensions_.end() ?
      NULL : maybe_extension->second.get();
}

void MockExtensionService::AddExtensionWithId(
    const std::string& id, Extension::Type type) {
  std::set<std::string> empty_permissions;
  AddExtensionWithIdAndPermissions(id, type, empty_permissions);
}

void MockExtensionService::AddExtensionWithIdAndPermissions(
    const std::string& id,
    Extension::Type type,
    const std::set<std::string>& permissions_set) {
  DictionaryValue manifest;
  manifest.SetString("name", std::string("Test extension ") + id);
  manifest.SetString("version", "1.0");

  scoped_ptr<ListValue> permissions(new ListValue());
  for (std::set<std::string>::const_iterator it = permissions_set.begin();
      it != permissions_set.end(); ++it) {
    permissions->Append(Value::CreateStringValue(*it));
  }
  manifest.Set("permissions", permissions.release());

  switch (type) {
    case Extension::TYPE_EXTENSION:
      break;

    case Extension::TYPE_LEGACY_PACKAGED_APP: {
      DictionaryValue* app = new DictionaryValue();
      DictionaryValue* app_launch = new DictionaryValue();
      app_launch->SetString("local_path", "fake.html");
      app->Set("launch", app_launch);
      manifest.Set("app", app);
      break;
    }

    default:
      NOTREACHED();
  }

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      FilePath(),
      Extension::INTERNAL,
      manifest,
      Extension::NO_FLAGS,
      id,
      &error));
  DCHECK(extension.get());
  DCHECK(error.empty());
  extensions_[id] = extension;

  for (std::set<std::string>::const_iterator it = permissions_set.begin();
      it != permissions_set.end(); ++it) {
    DCHECK(extension->HasAPIPermission(*it));
  }
}

// MockExtensionSystem

MockExtensionSystem::MockExtensionSystem(Profile* profile)
      : TestExtensionSystem(profile) {}
MockExtensionSystem::~MockExtensionSystem() {}

EventRouter* MockExtensionSystem::event_router() {
  if (!event_router_.get())
    event_router_.reset(new EventRouter(profile_, NULL));
  return event_router_.get();
}

ExtensionService* MockExtensionSystem::extension_service() {
  ExtensionServiceInterface* as_interface =
      static_cast<ExtensionServiceInterface*>(&extension_service_);
  return static_cast<ExtensionService*>(as_interface);
}

ProfileKeyedService* BuildMockExtensionSystem(Profile* profile) {
  return new MockExtensionSystem(profile);
}

// MockProfile

MockProfile::MockProfile(const FilePath& file_path)
    : TestingProfile(file_path) {
  ExtensionSystemFactory::GetInstance()->SetTestingFactoryAndUse(this,
      &BuildMockExtensionSystem);
}

MockProfile::~MockProfile() {}

// ScopedSettingsFactory

ScopedSettingsStorageFactory::ScopedSettingsStorageFactory() {}

ScopedSettingsStorageFactory::ScopedSettingsStorageFactory(
    const scoped_refptr<SettingsStorageFactory>& delegate)
    : delegate_(delegate) {}

ScopedSettingsStorageFactory::~ScopedSettingsStorageFactory() {}

void ScopedSettingsStorageFactory::Reset(
    const scoped_refptr<SettingsStorageFactory>& delegate) {
  delegate_ = delegate;
}

ValueStore* ScopedSettingsStorageFactory::Create(
    const FilePath& base_path,
    const std::string& extension_id) {
  DCHECK(delegate_.get());
  return delegate_->Create(base_path, extension_id);
}

}  // namespace settings_test_util

}  // namespace extensions
