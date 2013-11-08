// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_data_manager_factory.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace autofill {
namespace {

class PersonalDataManagerServiceImpl : public PersonalDataManagerService {
 public:
  explicit PersonalDataManagerServiceImpl(Profile* profile);
  virtual ~PersonalDataManagerServiceImpl();

  // PersonalDataManagerService:
  virtual void Shutdown() OVERRIDE;
  virtual PersonalDataManager* GetPersonalDataManager() OVERRIDE;

 private:
  scoped_ptr<PersonalDataManager> personal_data_manager_;
};

PersonalDataManagerServiceImpl::PersonalDataManagerServiceImpl(
    Profile* profile) {
  personal_data_manager_.reset(new PersonalDataManager(
      g_browser_process->GetApplicationLocale()));
  personal_data_manager_->Init(profile,
                               profile->GetPrefs(),
                               profile->IsOffTheRecord());
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
          GetInstance()->GetServiceForBrowserContext(profile, true));

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
    : BrowserContextKeyedServiceFactory(
        "PersonalDataManager",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(WebDataServiceFactory::GetInstance());
}

PersonalDataManagerFactory::~PersonalDataManagerFactory() {
}

BrowserContextKeyedService* PersonalDataManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  PersonalDataManagerService* service =
      new PersonalDataManagerServiceImpl(static_cast<Profile*>(profile));
  return service;
}

content::BrowserContext* PersonalDataManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace autofill
