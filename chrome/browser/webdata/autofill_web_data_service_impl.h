// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_WEB_DATA_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEBDATA_AUTOFILL_WEB_DATA_SERVICE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/api/webdata/autofill_web_data_service.h"
#include "chrome/browser/api/webdata/web_data_results.h"
#include "chrome/browser/api/webdata/web_data_service_base.h"
#include "chrome/browser/api/webdata/web_data_service_consumer.h"
#include "chrome/browser/webdata/web_database.h"

class AutofillChange;
class WebDatabaseService;

// This aggregates a WebDataService and delegates all method calls to
// it.
class AutofillWebDataServiceImpl : public AutofillWebDataService {
 public:
  AutofillWebDataServiceImpl(scoped_refptr<WebDatabaseService> wdbs,
                             const ProfileErrorCallback& callback);

  // WebDataServiceBase overrides:
  virtual content::NotificationSource GetNotificationSource() OVERRIDE;

  // AutofillWebData implementation.
  virtual void AddFormFields(
      const std::vector<FormFieldData>& fields) OVERRIDE;
  virtual WebDataServiceBase::Handle GetFormValuesForElementName(
      const string16& name,
      const string16& prefix,
      int limit,
      WebDataServiceConsumer* consumer) OVERRIDE;
  virtual void RemoveFormElementsAddedBetween(
      const base::Time& delete_begin, const base::Time& delete_end) OVERRIDE;
  virtual void RemoveExpiredFormElements() OVERRIDE;
  virtual void RemoveFormValueForElementName(const string16& name,
                                             const string16& value) OVERRIDE;
  virtual void AddAutofillProfile(const AutofillProfile& profile) OVERRIDE;
  virtual void UpdateAutofillProfile(const AutofillProfile& profile) OVERRIDE;
  virtual void RemoveAutofillProfile(const std::string& guid) OVERRIDE;
  virtual WebDataServiceBase::Handle GetAutofillProfiles(
      WebDataServiceConsumer* consumer) OVERRIDE;
  virtual void AddCreditCard(const CreditCard& credit_card) OVERRIDE;
  virtual void UpdateCreditCard(const CreditCard& credit_card) OVERRIDE;
  virtual void RemoveCreditCard(const std::string& guid) OVERRIDE;
  virtual WebDataServiceBase::Handle GetCreditCards(
      WebDataServiceConsumer* consumer) OVERRIDE;
  virtual void RemoveAutofillDataModifiedBetween(
      const base::Time& delete_begin, const base::Time& delete_end) OVERRIDE;


 protected:
  virtual ~AutofillWebDataServiceImpl();

 private:
  WebDatabase::State AddFormElementsImpl(
      const std::vector<FormFieldData>& fields, WebDatabase* db);
  scoped_ptr<WDTypedResult> GetFormValuesForElementNameImpl(
      const string16& name, const string16& prefix, int limit, WebDatabase* db);
  WebDatabase::State RemoveFormElementsAddedBetweenImpl(
      const base::Time& delete_begin, const base::Time& delete_end,
      WebDatabase* db);
  WebDatabase::State RemoveExpiredFormElementsImpl(WebDatabase* db);
  WebDatabase::State RemoveFormValueForElementNameImpl(
      const string16& name, const string16& value, WebDatabase* db);
  WebDatabase::State AddAutofillProfileImpl(
      const AutofillProfile& profile, WebDatabase* db);
  WebDatabase::State UpdateAutofillProfileImpl(
      const AutofillProfile& profile, WebDatabase* db);
  WebDatabase::State RemoveAutofillProfileImpl(
      const std::string& guid, WebDatabase* db);
  scoped_ptr<WDTypedResult> GetAutofillProfilesImpl(WebDatabase* db);
  WebDatabase::State AddCreditCardImpl(
      const CreditCard& credit_card, WebDatabase* db);
  WebDatabase::State UpdateCreditCardImpl(
      const CreditCard& credit_card, WebDatabase* db);
  WebDatabase::State RemoveCreditCardImpl(
      const std::string& guid, WebDatabase* db);
  scoped_ptr<WDTypedResult> GetCreditCardsImpl(WebDatabase* db);
  WebDatabase::State RemoveAutofillDataModifiedBetweenImpl(
      const base::Time& delete_begin, const base::Time& delete_end,
      WebDatabase* db);

  // Callbacks to ensure that sensitive info is destroyed if request is
  // cancelled.
  void DestroyAutofillProfileResult(const WDTypedResult* result);
  void DestroyAutofillCreditCardResult(const WDTypedResult* result);

  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataServiceImpl);
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_WEB_DATA_SERVICE_IMPL_H_
