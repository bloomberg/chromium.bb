// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_API_WEBDATA_AUTOFILL_WEB_DATA_H_
#define CHROME_BROWSER_API_WEBDATA_AUTOFILL_WEB_DATA_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/api/webdata/web_data_service_base.h"

namespace webkit {
namespace forms {
struct FormField;
}
}

class AutofillProfile;
class CreditCard;
class Profile;
class WebDataServiceConsumer;

// Pure virtual interface for retrieving Autofill data.  API users
// should use AutofillWebDataService.
class AutofillWebData {
 public:
  virtual ~AutofillWebData() {}

  // Schedules a task to add form fields to the web database.
  virtual void AddFormFields(
      const std::vector<webkit::forms::FormField>& fields) = 0;

  // Initiates the request for a vector of values which have been entered in
  // form input fields named |name|.  The method OnWebDataServiceRequestDone of
  // |consumer| gets called back when the request is finished, with the vector
  // included in the argument |result|.
  virtual WebDataServiceBase::Handle GetFormValuesForElementName(
      const string16& name,
      const string16& prefix,
      int limit,
      WebDataServiceConsumer* consumer) = 0;

  virtual void RemoveExpiredFormElements() = 0;
  virtual void RemoveFormValueForElementName(const string16& name,
                                             const string16& value) = 0;

  // Schedules a task to add an Autofill profile to the web database.
  virtual void AddAutofillProfile(const AutofillProfile& profile) = 0;

  // Schedules a task to update an Autofill profile in the web database.
  virtual void UpdateAutofillProfile(const AutofillProfile& profile) = 0;

  // Schedules a task to remove an Autofill profile from the web database.
  // |guid| is the identifer of the profile to remove.
  virtual void RemoveAutofillProfile(const std::string& guid) = 0;

  // Initiates the request for all Autofill profiles.  The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the profiles included in the argument |result|.  The
  // consumer owns the profiles.
  virtual WebDataServiceBase::Handle GetAutofillProfiles(
      WebDataServiceConsumer* consumer) = 0;

  // Remove "trashed" profile guids from the web database and optionally send
  // notifications to tell Sync that the items have been removed.
  virtual void EmptyMigrationTrash(bool notify_sync) = 0;

  // Schedules a task to add credit card to the web database.
  virtual void AddCreditCard(const CreditCard& credit_card) = 0;

  // Schedules a task to update credit card in the web database.
  virtual void UpdateCreditCard(const CreditCard& credit_card) = 0;

  // Schedules a task to remove a credit card from the web database.
  // |guid| is identifer of the credit card to remove.
  virtual void RemoveCreditCard(const std::string& guid) = 0;

  // Initiates the request for all credit cards.  The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the credit cards included in the argument |result|.  The
  // consumer owns the credit cards.
  virtual WebDataServiceBase::Handle
      GetCreditCards(WebDataServiceConsumer* consumer) = 0;
};

#endif  // CHROME_BROWSER_API_WEBDATA_AUTOFILL_WEB_DATA_H_
