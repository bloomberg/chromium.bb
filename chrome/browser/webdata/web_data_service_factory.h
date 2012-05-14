// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_FACTORY_H__
#define CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_FACTORY_H__
#pragma once

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

class WebDataService;

class WebDataServiceFactory : public RefcountedProfileKeyedServiceFactory {
 public:
  // Returns the |WebDataService| associated with the |profile|.
  // |access_type| is either EXPLICIT_ACCESS or IMPLICIT_ACCESS
  // (see its definition).
  static scoped_refptr<WebDataService> GetForProfile(
      Profile* profile, Profile::ServiceAccessType access_type);

  // Similar to GetForProfile(), but won't create the web data service if it
  // doesn't already exist.
  static scoped_refptr<WebDataService> GetForProfileIfExists(
      Profile* profile, Profile::ServiceAccessType access_type);

  static WebDataServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<WebDataServiceFactory>;

  WebDataServiceFactory();
  virtual ~WebDataServiceFactory();

  // |ProfileKeyedBaseFactory| methods:
  virtual bool ServiceRedirectedInIncognito() OVERRIDE;
  virtual scoped_refptr<RefcountedProfileKeyedService> BuildServiceInstanceFor(
        Profile* profile) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() OVERRIDE;
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_FACTORY_H__
