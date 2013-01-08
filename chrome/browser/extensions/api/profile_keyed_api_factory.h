// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROFILE_KEYED_API_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROFILE_KEYED_API_FACTORY_H_

#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

template <typename T>
class ProfileKeyedAPIFactory;

// Instantiations of ProfileKeyedAPIFactory should use this base class
// and also define a static const char* service_name() function (used in the
// ProfileKeyedBaseFactory constructor). These fields should be accessible
// to the ProfileKeyedAPIFactory for the service.
class ProfileKeyedAPI : public ProfileKeyedService {
 protected:
  // Defaults for flags that control ProfileKeyedAPIFactory behavior.
  // See ProfileKeyedBaseFactory for usage.
  static const bool kServiceRedirectedInIncognito = false;
  static const bool kServiceIsCreatedWithProfile = true;
  static const bool kServiceIsNULLWhileTesting = false;

  // Users of this factory template must define a GetFactoryInstance()
  // and manage their own instances (typically using LazyInstance or
  // Singleton), because those cannot be included in more than one
  // translation unit (and thus cannot be initialized in a header file).
  //
  // In the header file, declare GetFactoryInstance(), e.g.:
  //   class ProcessesAPI {
  //   ...
  //    public:
  //     static ProfileKeyedAPIFactory<ProcessesAPI>* GetFactoryInstance();
  //   };
  //
  // In the cc file, provide the implementation, e.g.:
  //   static base::LazyInstance<ProfileKeyedAPIFactory<ProcessesAPI> >
  //   g_factory = LAZY_INSTANCE_INITIALIZER;
  //
  //   // static
  //   ProfileKeyedAPIFactory<ProcessesAPI>*
  //   ProcessesAPI::GetFactoryInstance() {
  //     return &g_factory.Get();
  //   }
};

// A template for factories for ProfileKeyedServices that manage extension APIs.
// T is a ProfileKeyedService that uses this factory template instead of
// its own separate factory definition to manage its per-profile instances.
template <typename T>
class ProfileKeyedAPIFactory : public ProfileKeyedServiceFactory {
 public:
  static T* GetForProfile(Profile* profile) {
    return static_cast<T*>(
        T::GetFactoryInstance()->GetServiceForProfile(profile, true));
  }

  // Declare dependencies on other factories.
  // By default, ExtensionSystemFactory is the only dependency; however,
  // specializations can override this. Declare your specialization in
  // your header file after the ProfileKeyedAPI class definition.
  // Then in the cc file (or inline in the header), define it, e.g.:
  //   template <>
  //   ProfileKeyedAPIFactory<PushMessagingAPI>::DeclareFactoryDependencies() {
  //     DependsOn(ExtensionSystemFactory::GetInstance());
  //     DependsOn(ProfileSyncServiceFactory::GetInstance());
  // }
  void DeclareFactoryDependencies() {
    DependsOn(ExtensionSystemFactory::GetInstance());
  }

  ProfileKeyedAPIFactory()
  : ProfileKeyedServiceFactory(T::service_name(),
                               ProfileDependencyManager::GetInstance()) {
    DeclareFactoryDependencies();
  }

  virtual ~ProfileKeyedAPIFactory() {
  }

 private:
  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE {
    return new T(profile);
  }

  // ProfileKeyedBaseFactory implementation.
  // These can be effectively overridden with template specializations.
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE {
    return T::kServiceRedirectedInIncognito;
  }

  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE {
    return T::kServiceIsCreatedWithProfile;
  }

  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE {
    return T::kServiceIsNULLWhileTesting;
  }

  DISALLOW_COPY_AND_ASSIGN(ProfileKeyedAPIFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROFILE_KEYED_API_FACTORY_H_
