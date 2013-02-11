// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/common/pref_names.h"

LoginUIServiceFactory::LoginUIServiceFactory()
    : ProfileKeyedServiceFactory("LoginUIServiceFactory",
                                 ProfileDependencyManager::GetInstance()) {
}

LoginUIServiceFactory::~LoginUIServiceFactory() {}

// static
LoginUIService* LoginUIServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LoginUIService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
LoginUIServiceFactory* LoginUIServiceFactory::GetInstance() {
  return Singleton<LoginUIServiceFactory>::get();
}

ProfileKeyedService* LoginUIServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new LoginUIService(profile);
}
