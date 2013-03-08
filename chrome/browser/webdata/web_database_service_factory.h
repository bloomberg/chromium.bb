// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_FACTORY_H__
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_FACTORY_H__

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class WebDatabaseService;

namespace content {
  class BrowserContext;
}

// Singleton that owns all WebDatabaseService and associates them with
// Profiles.
class WebDatabaseServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static WebDatabaseService* FromBrowserContext(
    content::BrowserContext* context);
  // Returns the |WebDatabaseService| associated with the |profile|.
  // |access_type| is either EXPLICIT_ACCESS or IMPLICIT_ACCESS
  // (see its definition).
  static WebDatabaseService* GetForProfile(
      Profile* profile, Profile::ServiceAccessType access_type);

  // Similar to GetForProfile(), but won't create the web data service if it
  // doesn't already exist.
  static WebDatabaseService* GetForProfileIfExists(
      Profile* profile, Profile::ServiceAccessType access_type);

  static WebDatabaseServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<WebDatabaseServiceFactory>;

  WebDatabaseServiceFactory();
  virtual ~WebDatabaseServiceFactory();

  // |ProfileKeyedBaseFactory| methods:
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_FACTORY_H__
