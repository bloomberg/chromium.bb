// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_
#define COMPONENTS_AUTOFILL_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "components/autofill/browser/webdata/autofill_webdata.h"
#include "components/autofill/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/common/form_field_data.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"

class WebDataServiceBackend;

namespace autofill {

class AutofillChange;
class AutofillProfile;
class AutofillWebDataServiceObserverOnDBThread;
class CreditCard;

// Backend implentation for the AutofillWebDataService. This class runs on the
// DB thread, as it handles reads and writes to the WebDatabase, and functions
// in it should only be called from that thread. Most functions here are just
// the implementations of the corresponding functions in the Autofill
// WebDataService.
class AutofillWebDataBackendImpl
    : public base::RefCountedThreadSafe<AutofillWebDataBackendImpl,
          content::BrowserThread::DeleteOnDBThread>,
      public AutofillWebDataBackend {
 public:
  // |web_database_backend| is used to access the WebDatabase directly for
  // Sync-related operations. |on_changed_callback| is a closure which can be
  // used to notify the UI thread of changes initiated by Sync (this callback
  // may be called multiple times).
  AutofillWebDataBackendImpl(
      scoped_refptr<WebDataServiceBackend> web_database_backend,
      const base::Closure& on_changed_callback);

  // AutofillWebDataBackend implementation.
  virtual void AddObserver(AutofillWebDataServiceObserverOnDBThread* observer)
      OVERRIDE;
  virtual void RemoveObserver(
      AutofillWebDataServiceObserverOnDBThread* observer) OVERRIDE;
  virtual WebDatabase* GetDatabase() OVERRIDE;
  virtual void RemoveExpiredFormElementsWrapper() OVERRIDE;
  virtual void NotifyOfMultipleAutofillChanges() OVERRIDE;

  // Adds form fields to the web database.
  WebDatabase::State AddFormElements(const std::vector<FormFieldData>& fields,
                                     WebDatabase* db);

  // Returns a vector of values which have been entered in form input fields
  // named |name|.
  scoped_ptr<WDTypedResult> GetFormValuesForElementName(
      const base::string16& name,
      const base::string16& prefix,
      int limit,
      WebDatabase* db);

  // Returns true if there are any elements in the form.
  scoped_ptr<WDTypedResult> HasFormElements(WebDatabase* db);

  // Removes form elements recorded for Autocomplete from the database.
  WebDatabase::State RemoveFormElementsAddedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      WebDatabase* db);

  // Removes expired form elements recorded for Autocomplete from the database.
  WebDatabase::State RemoveExpiredFormElements(WebDatabase* db);

  // Removes the Form-value |value| which has been entered in form input fields
  // named |name| from the database.
  WebDatabase::State RemoveFormValueForElementName(const base::string16& name,
                                                   const base::string16& value,
                                                   WebDatabase* db);

  // Adds an Autofill profile to the web database.
  WebDatabase::State AddAutofillProfile(const AutofillProfile& profile,
                                        WebDatabase* db);

  // Updates an Autofill profile in the web database.
  WebDatabase::State UpdateAutofillProfile(const AutofillProfile& profile,
                                           WebDatabase* db);

  // Removes an Autofill profile from the web database.
  WebDatabase::State RemoveAutofillProfile(const std::string& guid,
                                           WebDatabase* db);

  // Returns all Autofill profiles from the web database.
  scoped_ptr<WDTypedResult> GetAutofillProfiles(WebDatabase* db);

  // Adds a credit card to the web database.
  WebDatabase::State AddCreditCard(const CreditCard& credit_card,
                                   WebDatabase* db);

  // Updates a credit card in the web database.
  WebDatabase::State UpdateCreditCard(const CreditCard& credit_card,
                                      WebDatabase* db);

  // Removes a credit card from the web database.
  WebDatabase::State RemoveCreditCard(const std::string& guid,
                                      WebDatabase* db);

  // Returns a vector of all credit cards from the web database.
  scoped_ptr<WDTypedResult> GetCreditCards(WebDatabase* db);

  // Removes Autofill records from the database.
  WebDatabase::State RemoveAutofillDataModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      WebDatabase* db);

 protected:
  virtual ~AutofillWebDataBackendImpl();

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::DB>;
  friend class base::DeleteHelper<AutofillWebDataBackendImpl>;
  // We have to friend RCTS<> so WIN shared-lib build is happy
  // (http://crbug/112250).
  friend class base::RefCountedThreadSafe<AutofillWebDataBackendImpl,
      content::BrowserThread::DeleteOnDBThread>;

  // Callbacks to ensure that sensitive info is destroyed if request is
  // cancelled.
  void DestroyAutofillProfileResult(const WDTypedResult* result);
  void DestroyAutofillCreditCardResult(const WDTypedResult* result);

  ObserverList<AutofillWebDataServiceObserverOnDBThread> db_observer_list_;

  // WebDataServiceBackend allows direct access to DB.
  // TODO(caitkp): Make it so nobody but us needs direct DB access anymore.
  scoped_refptr<WebDataServiceBackend> web_database_backend_;

  base::Closure on_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataBackendImpl);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_
