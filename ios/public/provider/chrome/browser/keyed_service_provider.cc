// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/keyed_service_provider.h"

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

namespace ios {

KeyedServiceProvider::KeyedServiceProvider() {
}

KeyedServiceProvider::~KeyedServiceProvider() {
}

scoped_refptr<autofill::AutofillWebDataService>
KeyedServiceProvider::GetAutofillWebDataForBrowserState(
    ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  return nullptr;
}

autofill::PersonalDataManager*
KeyedServiceProvider::GetPersonalDataManagerForBrowserState(
    ChromeBrowserState* browser_state) {
  return nullptr;
}

}  // namespace ios
