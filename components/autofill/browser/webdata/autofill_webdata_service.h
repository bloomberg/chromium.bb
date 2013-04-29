// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_
#define COMPONENTS_AUTOFILL_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/autofill/browser/webdata/autofill_webdata.h"
#include "components/autofill/common/form_field_data.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"

class WebDatabaseService;

namespace content {
class BrowserContext;
}

namespace autofill {

class AutofillChange;
class AutofillProfile;
class AutofillWebDataServiceObserverOnDBThread;
class AutofillWebDataServiceObserverOnUIThread;
class CreditCard;

// API for Autofill web data.
class AutofillWebDataService : public AutofillWebData,
                               public WebDataServiceBase {
 public:
  AutofillWebDataService();

  AutofillWebDataService(scoped_refptr<WebDatabaseService> wdbs,
                         const ProfileErrorCallback& callback);

  // Retrieve an AutofillWebDataService for the given context.
  // Can return NULL in some contexts.
  static scoped_refptr<AutofillWebDataService> FromBrowserContext(
      content::BrowserContext* context);

  // Notifies listeners on the UI thread that multiple changes have been made to
  // to Autofill records of the database.
  // NOTE: This method is intended to be called from the DB thread.  It
  // it asynchronously notifies listeners on the UI thread.
  // |web_data_service| may be NULL for testing purposes.
  static void NotifyOfMultipleAutofillChanges(
      AutofillWebDataService* web_data_service);

  // WebDataServiceBase implementation.
  virtual void ShutdownOnUIThread() OVERRIDE;

  // AutofillWebData implementation.
  virtual void AddFormFields(
      const std::vector<FormFieldData>& fields) OVERRIDE;
  virtual WebDataServiceBase::Handle GetFormValuesForElementName(
      const base::string16& name,
      const base::string16& prefix,
      int limit,
      WebDataServiceConsumer* consumer) OVERRIDE;
  virtual void RemoveFormElementsAddedBetween(
      const base::Time& delete_begin, const base::Time& delete_end) OVERRIDE;
  virtual void RemoveExpiredFormElements() OVERRIDE;
  virtual void RemoveFormValueForElementName(
      const base::string16& name,
      const base::string16& value) OVERRIDE;
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

  void AddObserver(AutofillWebDataServiceObserverOnDBThread* observer);
  void RemoveObserver(AutofillWebDataServiceObserverOnDBThread* observer);

  void AddObserver(AutofillWebDataServiceObserverOnUIThread* observer);
  void RemoveObserver(AutofillWebDataServiceObserverOnUIThread* observer);

  // Returns a SupportsUserData objects that may be used to store data
  // owned by the DB thread on this object. Should be called only from
  // the DB thread, and will be destroyed on the DB thread soon after
  // |ShutdownOnUIThread()| is called.
  base::SupportsUserData* GetDBUserData();

 protected:
  virtual ~AutofillWebDataService();

  virtual void ShutdownOnDBThread();

 private:
  WebDatabase::State AddFormElementsImpl(
      const std::vector<FormFieldData>& fields, WebDatabase* db);
  scoped_ptr<WDTypedResult> GetFormValuesForElementNameImpl(
      const base::string16& name, const base::string16& prefix, int limit,
      WebDatabase* db);
  WebDatabase::State RemoveFormElementsAddedBetweenImpl(
      const base::Time& delete_begin, const base::Time& delete_end,
      WebDatabase* db);
  WebDatabase::State RemoveExpiredFormElementsImpl(WebDatabase* db);
  WebDatabase::State RemoveFormValueForElementNameImpl(
      const base::string16& name, const base::string16& value, WebDatabase* db);
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

  void NotifyAutofillMultipleChangedOnUIThread();

  // This makes the destructor public, and thus allows us to aggregate
  // SupportsUserData. It is private by default to prevent incorrect
  // usage in class hierarchies where it is inherited by
  // reference-counted objects.
  class SupportsUserDataAggregatable : public base::SupportsUserData {
   public:
    SupportsUserDataAggregatable() {}
    virtual ~SupportsUserDataAggregatable() {}
   private:
    DISALLOW_COPY_AND_ASSIGN(SupportsUserDataAggregatable);
  };

  // Storage for user data to be accessed only on the DB thread. May
  // be used e.g. for SyncableService subclasses that need to be owned
  // by this object. Is created on first call to |GetDBUserData()|.
  scoped_ptr<SupportsUserDataAggregatable> db_thread_user_data_;

  ObserverList<AutofillWebDataServiceObserverOnDBThread> db_observer_list_;
  ObserverList<AutofillWebDataServiceObserverOnUIThread> ui_observer_list_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataService);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_
