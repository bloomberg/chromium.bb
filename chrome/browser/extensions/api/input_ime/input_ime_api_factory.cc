// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_api_factory.h"

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
InputImeAPIFactory* InputImeAPIFactory::GetInstance() {
  return Singleton<InputImeAPIFactory>::get();
}

InputImeAPIFactory::InputImeAPIFactory()
    : ProfileKeyedServiceFactory("InputImeAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

InputImeAPIFactory::~InputImeAPIFactory() {
}

ProfileKeyedService* InputImeAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new InputImeAPI(profile);
}

bool InputImeAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool InputImeAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
