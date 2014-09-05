// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/autofill_webdata.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"

namespace base {
class MessageLoopProxy;
}

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
// This class is destroyed on the DB thread.
class AutofillWebDataBackendImpl
    : public base::RefCountedDeleteOnMessageLoop<AutofillWebDataBackendImpl>,
      public AutofillWebDataBackend {
 public:
  // |web_database_backend| is used to access the WebDatabase directly for
  // Sync-related operations. |ui_thread| and |db_thread| are the threads that
  // this class uses as its UI and DB threads respectively.
  // |on_changed_callback| is a closure which can be used to notify the UI
  // thread of changes initiated by Sync (this callback may be called multiple
  // times).
  AutofillWebDataBackendImpl(
      scoped_refptr<WebDataServiceBackend> web_database_backend,
      scoped_refptr<base::MessageLoopProxy> ui_thread,
      scoped_refptr<base::MessageLoopProxy> db_thread,
      const base::Closure& on_changed_callback);

  // AutofillWebDataBackend implementation.
  virtual void AddObserver(AutofillWebDataServiceObserverOnDBThread* observer)
      OVERRIDE;
  virtual void RemoveObserver(
      AutofillWebDataServiceObserverOnDBThread* observer) OVERRIDE;
  virtual WebDatabase* GetDatabase() OVERRIDE;
  virtual void RemoveExpiredFormElements() OVERRIDE;
  virtual void NotifyOfMultipleAutofillChanges() OVERRIDE;

  // Returns a SupportsUserData objects that may be used to store data
  // owned by the DB thread on this object. Should be called only from
  // the DB thread, and will be destroyed on the DB thread soon after
  // |ShutdownOnUIThread()| is called.
  base::SupportsUserData* GetDBUserData();

  void ResetUserData();

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

  // Updates Autofill entries in the web database.
  WebDatabase::State UpdateAutofillEntries(
      const std::vector<autofill::AutofillEntry>& autofill_entries,
      WebDatabase* db);

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

  // Removes origin URLs associated with Autofill profiles and credit cards from
  // the database.
  WebDatabase::State RemoveOriginURLsModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      WebDatabase* db);

 protected:
  virtual ~AutofillWebDataBackendImpl();

 private:
  friend class base::RefCountedDeleteOnMessageLoop<AutofillWebDataBackendImpl>;
  friend class base::DeleteHelper<AutofillWebDataBackendImpl>;

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

  // The MessageLoopProxy that this class uses as its UI thread.
  scoped_refptr<base::MessageLoopProxy> ui_thread_;

  // The MessageLoopProxy that this class uses as its DB thread.
  scoped_refptr<base::MessageLoopProxy> db_thread_;

  // Storage for user data to be accessed only on the DB thread. May
  // be used e.g. for SyncableService subclasses that need to be owned
  // by this object. Is created on first call to |GetDBUserData()|.
  scoped_ptr<SupportsUserDataAggregatable> user_data_;

  WebDatabase::State RemoveExpiredFormElementsImpl(WebDatabase* db);

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

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_IMPL_H_
