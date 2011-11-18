// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_test_util.h"

#include "base/file_path.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"

namespace extensions {

namespace settings_test_util {

// Intended as a StorageCallback from GetStorage.
static void AssignStorage(SettingsStorage** dst, SettingsStorage* src) {
  *dst = src;
}

SettingsStorage* GetStorage(
    const std::string& extension_id, SettingsFrontend* frontend) {
  SettingsStorage* storage = NULL;
  frontend->RunWithStorage(
      extension_id,
      base::Bind(&AssignStorage, &storage));
  MessageLoop::current()->RunAllPending();
  return storage;
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
  DictionaryValue manifest;
  manifest.SetString("name", std::string("Test extension ") + id);
  manifest.SetString("version", "1.0");

  switch (type) {
    case Extension::TYPE_EXTENSION:
      break;

    case Extension::TYPE_PACKAGED_APP: {
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
  extensions_[id] = Extension::CreateWithId(
      FilePath(),
      Extension::INTERNAL,
      manifest,
      Extension::NO_FLAGS,
      id,
      &error);
  DCHECK(error.empty());
}

// MockProfile

MockProfile::MockProfile(const FilePath& file_path)
    : TestingProfile(file_path) {
  event_router_.reset(new ExtensionEventRouter(this));
}

MockProfile::~MockProfile() {}

MockExtensionService* MockProfile::GetMockExtensionService() {
  return &extension_service_;
}

ExtensionService* MockProfile::GetExtensionService() {
  ExtensionServiceInterface* as_interface =
      static_cast<ExtensionServiceInterface*>(&extension_service_);
  return static_cast<ExtensionService*>(as_interface);
}

ExtensionEventRouter* MockProfile::GetExtensionEventRouter() {
  return event_router_.get();
}

// ScopedSettingsFactory

ScopedSettingsStorageFactory::ScopedSettingsStorageFactory(
    SettingsStorageFactory* delegate) : delegate_(delegate) {
  DCHECK(delegate);
}

ScopedSettingsStorageFactory::~ScopedSettingsStorageFactory() {}

void ScopedSettingsStorageFactory::Reset(SettingsStorageFactory* delegate) {
  DCHECK(delegate);
  delegate_.reset(delegate);
}

SettingsStorage* ScopedSettingsStorageFactory::Create(
    const FilePath& base_path, const std::string& extension_id) {
  return delegate_->Create(base_path, extension_id);
}

}  // namespace settings_test_util

}  // namespace extensions
