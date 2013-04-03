// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_FACTORY_H__
#define CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_FACTORY_H__

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/webdata/common/web_database_service.h"

class AutofillWebDataService;
class WebDataService;

// A wrapper of WebDataService so that we can use it as a profile keyed service.
class WebDataServiceWrapper : public ProfileKeyedService {
 public:
  explicit WebDataServiceWrapper(Profile* profile);

  // For testing.
  WebDataServiceWrapper();

  virtual ~WebDataServiceWrapper();

  // ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  virtual scoped_refptr<AutofillWebDataService> GetAutofillWebData();

  virtual scoped_refptr<WebDataService> GetWebData();

 private:
  scoped_refptr<WebDatabaseService> web_database_;

  scoped_refptr<AutofillWebDataService> autofill_web_data_;
  scoped_refptr<WebDataService> web_data_;

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceWrapper);
};

// Singleton that owns all WebDataServiceWrappers and associates them with
// Profiles.
class WebDataServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the |WebDataServiceWrapper| associated with the |profile|.
  // |access_type| is either EXPLICIT_ACCESS or IMPLICIT_ACCESS
  // (see its definition).
  static WebDataServiceWrapper* GetForProfile(
      Profile* profile, Profile::ServiceAccessType access_type);

  static WebDataServiceWrapper* GetForProfileIfExists(
      Profile* profile, Profile::ServiceAccessType access_type);

  static WebDataServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<WebDataServiceFactory>;

  WebDataServiceFactory();
  virtual ~WebDataServiceFactory();

  // |ProfileKeyedBaseFactory| methods:
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceFactory);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_FACTORY_H__
