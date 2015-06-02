// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"

enum class ServiceAccessType;

namespace autofill {
class AutofillWebDataService;
class PersonalDataManager;
}

namespace ios {

class ChromeBrowserState;

// A class that provides access to KeyedService that do not have a pure iOS
// implementation yet.
class KeyedServiceProvider {
 public:
  KeyedServiceProvider();
  virtual ~KeyedServiceProvider();

  // Returns an instance of autofill::AutofillWebDataService tied to
  // |browser_state|.
  virtual scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForBrowserState(ChromeBrowserState* browser_state,
                                    ServiceAccessType access_type);

  // Returns an instance of autofill::PersonalDataManager tied to
  // |browser_state|.
  virtual autofill::PersonalDataManager* GetPersonalDataManagerForBrowserState(
      ChromeBrowserState* browser_state);

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyedServiceProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_
