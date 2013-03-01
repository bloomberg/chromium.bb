// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_data_manager_factory.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/webdata/web_data_service_factory.h"

namespace {

class PersonalDataManagerServiceImpl : public PersonalDataManagerService {
 public:
  PersonalDataManagerServiceImpl(Profile* profile);
  virtual ~PersonalDataManagerServiceImpl();

  // PersonalDataManagerService:
  virtual void Shutdown() OVERRIDE;
  virtual PersonalDataManager* GetPersonalDataManager() OVERRIDE;

 private:
  scoped_ptr<PersonalDataManager> personal_data_manager_;
};

PersonalDataManagerServiceImpl::PersonalDataManagerServiceImpl(
    Profile* profile) {
  personal_data_manager_.reset(new PersonalDataManager());
  personal_data_manager_->Init(profile);
}

PersonalDataManagerServiceImpl::~PersonalDataManagerServiceImpl() {}

void PersonalDataManagerServiceImpl::Shutdown() {
  personal_data_manager_.reset();
}

PersonalDataManager* PersonalDataManagerServiceImpl::GetPersonalDataManager() {
  return personal_data_manager_.get();
}

}  // namespace

// static
PersonalDataManager* PersonalDataManagerFactory::GetForProfile(
    Profile* profile) {
  PersonalDataManagerService* service =
      static_cast<PersonalDataManagerService*>(
          GetInstance()->GetServiceForProfile(profile, true));

  if (service)
    return service->GetPersonalDataManager();

  // |service| can be NULL in Incognito mode.
  return NULL;
}

// static
PersonalDataManagerFactory* PersonalDataManagerFactory::GetInstance() {
  return Singleton<PersonalDataManagerFactory>::get();
}

PersonalDataManagerFactory::PersonalDataManagerFactory()
    : ProfileKeyedServiceFactory("PersonalDataManager",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(WebDataServiceFactory::GetInstance());
}

PersonalDataManagerFactory::~PersonalDataManagerFactory() {
}

ProfileKeyedService* PersonalDataManagerFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  PersonalDataManagerService* service =
      new PersonalDataManagerServiceImpl(profile);
  return service;
}
