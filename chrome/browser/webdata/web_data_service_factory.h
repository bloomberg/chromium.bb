// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_FACTORY_H__
#define CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_FACTORY_H__

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webdata/common/web_database_service.h"

class KeywordWebDataService;
class TokenWebData;

#if defined(OS_WIN)
class PasswordWebDataService;
#endif

namespace autofill {
class AutofillWebDataService;
}  // namespace autofill

// A wrapper of WebDataService so that we can use it as a profile keyed service.
class WebDataServiceWrapper : public KeyedService {
 public:
  explicit WebDataServiceWrapper(Profile* profile);

  // For testing.
  WebDataServiceWrapper();

  virtual ~WebDataServiceWrapper();

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

  virtual scoped_refptr<autofill::AutofillWebDataService> GetAutofillWebData();

  virtual scoped_refptr<KeywordWebDataService> GetKeywordWebData();

  virtual scoped_refptr<TokenWebData> GetTokenWebData();

#if defined(OS_WIN)
  virtual scoped_refptr<PasswordWebDataService> GetPasswordWebData();
#endif

 private:
  scoped_refptr<WebDatabaseService> web_database_;

  scoped_refptr<autofill::AutofillWebDataService> autofill_web_data_;
  scoped_refptr<KeywordWebDataService> keyword_web_data_;
  scoped_refptr<TokenWebData> token_web_data_;

#if defined(OS_WIN)
  scoped_refptr<PasswordWebDataService> password_web_data_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceWrapper);
};

// Singleton that owns all WebDataServiceWrappers and associates them with
// Profiles.
class WebDataServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the WebDataServiceWrapper associated with the |profile|.
  static WebDataServiceWrapper* GetForProfile(
      Profile* profile,
      Profile::ServiceAccessType access_type);

  static WebDataServiceWrapper* GetForProfileIfExists(
      Profile* profile,
      Profile::ServiceAccessType access_type);

  // Returns the AutofillWebDataService associated with the |profile|.
  static scoped_refptr<autofill::AutofillWebDataService>
      GetAutofillWebDataForProfile(Profile* profile,
                                   Profile::ServiceAccessType access_type);

  // Returns the KeywordWebDataService associated with the |profile|.
  static scoped_refptr<KeywordWebDataService> GetKeywordWebDataForProfile(
      Profile* profile,
      Profile::ServiceAccessType access_type);

  // Returns the TokenWebData associated with the |profile|.
  static scoped_refptr<TokenWebData> GetTokenWebDataForProfile(
      Profile* profile,
      Profile::ServiceAccessType access_type);

#if defined(OS_WIN)
  // Returns the PasswordWebDataService associated with the |profile|.
  static scoped_refptr<PasswordWebDataService> GetPasswordWebDataForProfile(
      Profile* profile,
      Profile::ServiceAccessType access_type);
#endif

  static WebDataServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<WebDataServiceFactory>;

  WebDataServiceFactory();
  virtual ~WebDataServiceFactory();

  // |BrowserContextKeyedBaseFactory| methods:
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceFactory);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_FACTORY_H__
