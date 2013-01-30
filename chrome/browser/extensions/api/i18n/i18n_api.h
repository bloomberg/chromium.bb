// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"

class Profile;

namespace extensions {

class I18nGetAcceptLanguagesFunction : public SyncExtensionFunction {
  virtual ~I18nGetAcceptLanguagesFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("i18n.getAcceptLanguages", I18N_GETACCEPTLANGUAGES)
};

class I18nAPI : public ProfileKeyedAPI {
 public:
  explicit I18nAPI(Profile* profile);
  virtual ~I18nAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<I18nAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<I18nAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "I18nAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  DISALLOW_COPY_AND_ASSIGN(I18nAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_I18N_I18N_API_H_
