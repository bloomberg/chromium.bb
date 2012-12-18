// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/input_method_api_factory.h"

#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
InputMethodAPIFactory* InputMethodAPIFactory::GetInstance() {
  return Singleton<InputMethodAPIFactory>::get();
}

InputMethodAPIFactory::InputMethodAPIFactory()
    : ProfileKeyedServiceFactory("InputMethodAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

InputMethodAPIFactory::~InputMethodAPIFactory() {
}

ProfileKeyedService* InputMethodAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new InputMethodAPI(profile);
}

bool InputMethodAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool InputMethodAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
