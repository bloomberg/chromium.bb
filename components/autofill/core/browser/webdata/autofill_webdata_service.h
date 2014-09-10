// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/autofill_webdata.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"

class WebDatabaseService;

namespace base {
class MessageLoopProxy;
}

namespace autofill {

class AutofillChange;
class AutofillEntry;
class AutofillProfile;
class AutofillWebDataBackend;
class AutofillWebDataBackendImpl;
class AutofillWebDataServiceObserverOnDBThread;
class AutofillWebDataServiceObserverOnUIThread;
class CreditCard;

// API for Autofill web data.
class AutofillWebDataService : public AutofillWebData,
                               public WebDataServiceBase {
 public:
  AutofillWebDataService(scoped_refptr<base::MessageLoopProxy> ui_thread,
                         scoped_refptr<base::MessageLoopProxy> db_thread);
  AutofillWebDataService(scoped_refptr<WebDatabaseService> wdbs,
                         scoped_refptr<base::MessageLoopProxy> ui_thread,
                         scoped_refptr<base::MessageLoopProxy> db_thread,
                         const ProfileErrorCallback& callback);

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

  virtual WebDataServiceBase::Handle HasFormElements(
      WebDataServiceConsumer* consumer) OVERRIDE;
  virtual void RemoveFormElementsAddedBetween(
      const base::Time& delete_begin, const base::Time& delete_end) OVERRIDE;
  virtual void RemoveFormValueForElementName(
      const base::string16& name,
      const base::string16& value) OVERRIDE;
  virtual void AddAutofillProfile(const AutofillProfile& profile) OVERRIDE;
  virtual void UpdateAutofillProfile(const AutofillProfile& profile) OVERRIDE;
  virtual void RemoveAutofillProfile(const std::string& guid) OVERRIDE;
  virtual WebDataServiceBase::Handle GetAutofillProfiles(
      WebDataServiceConsumer* consumer) OVERRIDE;
  virtual void UpdateAutofillEntries(
      const std::vector<AutofillEntry>& autofill_entries) OVERRIDE;
  virtual void AddCreditCard(const CreditCard& credit_card) OVERRIDE;
  virtual void UpdateCreditCard(const CreditCard& credit_card) OVERRIDE;
  virtual void RemoveCreditCard(const std::string& guid) OVERRIDE;
  virtual WebDataServiceBase::Handle GetCreditCards(
      WebDataServiceConsumer* consumer) OVERRIDE;
  virtual void RemoveAutofillDataModifiedBetween(
      const base::Time& delete_begin, const base::Time& delete_end) OVERRIDE;
  virtual void RemoveOriginURLsModifiedBetween(
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

  // Takes a callback which will be called on the DB thread with a pointer to an
  // |AutofillWebdataBackend|. This backend can be used to access or update the
  // WebDatabase directly on the DB thread.
  void GetAutofillBackend(
      const base::Callback<void(AutofillWebDataBackend*)>& callback);

 protected:
  virtual ~AutofillWebDataService();

  virtual void NotifyAutofillMultipleChangedOnUIThread();

  base::WeakPtr<AutofillWebDataService> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  ObserverList<AutofillWebDataServiceObserverOnUIThread> ui_observer_list_;

    // The MessageLoopProxy that this class uses as its UI thread.
  scoped_refptr<base::MessageLoopProxy> ui_thread_;

  // The MessageLoopProxy that this class uses as its DB thread.
  scoped_refptr<base::MessageLoopProxy> db_thread_;

  scoped_refptr<AutofillWebDataBackendImpl> autofill_backend_;

  // This factory is used on the UI thread. All vended weak pointers are
  // invalidated in ShutdownOnUIThread().
  base::WeakPtrFactory<AutofillWebDataService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataService);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_
