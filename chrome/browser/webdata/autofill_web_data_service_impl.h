// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_WEB_DATA_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEBDATA_AUTOFILL_WEB_DATA_SERVICE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/api/webdata/autofill_web_data_service.h"

class WebDataService;

// This aggregates a WebDataService and delegates all method calls to
// it.
class AutofillWebDataServiceImpl : public AutofillWebDataService {
 public:
  explicit AutofillWebDataServiceImpl(scoped_refptr<WebDataService> service);

  // AutofillWebData implementation.
  virtual void AddFormFields(
      const std::vector<FormFieldData>& fields) OVERRIDE;
  virtual WebDataServiceBase::Handle GetFormValuesForElementName(
      const string16& name,
      const string16& prefix,
      int limit,
      WebDataServiceConsumer* consumer) OVERRIDE;
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
  virtual WebDataServiceBase::Handle
      GetCreditCards(WebDataServiceConsumer* consumer) OVERRIDE;

  // WebDataServiceBase overrides.
  // TODO(caitkp): remove these when we decouple
  // WebDatabaseService life cycle from WebData).
  virtual void CancelRequest(Handle h) OVERRIDE;
  virtual content::NotificationSource GetNotificationSource() OVERRIDE;
  virtual bool IsDatabaseLoaded() OVERRIDE;
  virtual WebDatabase* GetDatabase() OVERRIDE;


 protected:
  virtual ~AutofillWebDataServiceImpl();

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataServiceImpl);

  const scoped_refptr<WebDataService> service_;
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_WEB_DATA_SERVICE_IMPL_H_
