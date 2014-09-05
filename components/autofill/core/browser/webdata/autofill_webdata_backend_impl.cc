// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_webdata_backend_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_data_service_backend.h"

using base::Bind;
using base::Time;

namespace autofill {

AutofillWebDataBackendImpl::AutofillWebDataBackendImpl(
    scoped_refptr<WebDataServiceBackend> web_database_backend,
    scoped_refptr<base::MessageLoopProxy> ui_thread,
    scoped_refptr<base::MessageLoopProxy> db_thread,
    const base::Closure& on_changed_callback)
    : base::RefCountedDeleteOnMessageLoop<AutofillWebDataBackendImpl>(
          db_thread),
      ui_thread_(ui_thread),
      db_thread_(db_thread),
      web_database_backend_(web_database_backend),
      on_changed_callback_(on_changed_callback) {
}

void AutofillWebDataBackendImpl::AddObserver(
    AutofillWebDataServiceObserverOnDBThread* observer) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  db_observer_list_.AddObserver(observer);
}

void AutofillWebDataBackendImpl::RemoveObserver(
    AutofillWebDataServiceObserverOnDBThread* observer) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  db_observer_list_.RemoveObserver(observer);
}

AutofillWebDataBackendImpl::~AutofillWebDataBackendImpl() {
  DCHECK(!user_data_.get()); // Forgot to call ResetUserData?
}

WebDatabase* AutofillWebDataBackendImpl::GetDatabase() {
  DCHECK(db_thread_->BelongsToCurrentThread());
  return web_database_backend_->database();
}

void AutofillWebDataBackendImpl::RemoveExpiredFormElements() {
  web_database_backend_->ExecuteWriteTask(
      Bind(&AutofillWebDataBackendImpl::RemoveExpiredFormElementsImpl,
           this));
}

void AutofillWebDataBackendImpl::NotifyOfMultipleAutofillChanges() {
  DCHECK(db_thread_->BelongsToCurrentThread());
  ui_thread_->PostTask(FROM_HERE, on_changed_callback_);
}

base::SupportsUserData* AutofillWebDataBackendImpl::GetDBUserData() {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!user_data_)
    user_data_.reset(new SupportsUserDataAggregatable());
  return user_data_.get();
}

void AutofillWebDataBackendImpl::ResetUserData() {
  user_data_.reset();
}

WebDatabase::State AutofillWebDataBackendImpl::AddFormElements(
    const std::vector<FormFieldData>& fields, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  AutofillChangeList changes;
  if (!AutofillTable::FromWebDatabase(db)->AddFormFieldValues(
        fields, &changes)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Post the notifications including the list of affected keys.
  // This is sent here so that work resulting from this notification will be
  // done on the DB thread, and not the UI thread.
  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                    db_observer_list_,
                    AutofillEntriesChanged(changes));

  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetFormValuesForElementName(
    const base::string16& name, const base::string16& prefix, int limit,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  std::vector<base::string16> values;
  AutofillTable::FromWebDatabase(db)->GetFormValuesForElementName(
      name, prefix, &values, limit);
  return scoped_ptr<WDTypedResult>(
      new WDResult<std::vector<base::string16> >(AUTOFILL_VALUE_RESULT,
                                                 values));
}

scoped_ptr<WDTypedResult> AutofillWebDataBackendImpl::HasFormElements(
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  bool value = AutofillTable::FromWebDatabase(db)->HasFormElements();
  return scoped_ptr<WDTypedResult>(
      new WDResult<bool>(AUTOFILL_VALUE_RESULT, value));
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveFormElementsAddedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  AutofillChangeList changes;

  if (AutofillTable::FromWebDatabase(db)->RemoveFormElementsAddedBetween(
          delete_begin, delete_end, &changes)) {
    if (!changes.empty()) {
      // Post the notifications including the list of affected keys.
      // This is sent here so that work resulting from this notification
      // will be done on the DB thread, and not the UI thread.
      FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                        db_observer_list_,
                        AutofillEntriesChanged(changes));
    }
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveFormValueForElementName(
    const base::string16& name, const base::string16& value, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());

  if (AutofillTable::FromWebDatabase(db)->RemoveFormElement(name, value)) {
    AutofillChangeList changes;
    changes.push_back(
        AutofillChange(AutofillChange::REMOVE, AutofillKey(name, value)));

    // Post the notifications including the list of affected keys.
    FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                      db_observer_list_,
                      AutofillEntriesChanged(changes));

    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddAutofillProfile(
    const AutofillProfile& profile, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!AutofillTable::FromWebDatabase(db)->AddAutofillProfile(profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(
      AutofillProfileChange::ADD, profile.guid(), &profile);
  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                    db_observer_list_,
                    AutofillProfileChanged(change));

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateAutofillProfile(
    const AutofillProfile& profile, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  // Only perform the update if the profile exists.  It is currently
  // valid to try to update a missing profile.  We simply drop the write and
  // the caller will detect this on the next refresh.
  AutofillProfile* original_profile = NULL;
  if (!AutofillTable::FromWebDatabase(db)->GetAutofillProfile(profile.guid(),
      &original_profile)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  scoped_ptr<AutofillProfile> scoped_profile(original_profile);

  if (!AutofillTable::FromWebDatabase(db)->UpdateAutofillProfile(profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(
      AutofillProfileChange::UPDATE, profile.guid(), &profile);
  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                    db_observer_list_,
                    AutofillProfileChanged(change));

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveAutofillProfile(
    const std::string& guid, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  AutofillProfile* profile = NULL;
  if (!AutofillTable::FromWebDatabase(db)->GetAutofillProfile(guid, &profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  scoped_ptr<AutofillProfile> scoped_profile(profile);

  if (!AutofillTable::FromWebDatabase(db)->RemoveAutofillProfile(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::REMOVE, guid, NULL);
  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                    db_observer_list_,
                    AutofillProfileChanged(change));

  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetAutofillProfiles(
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  std::vector<AutofillProfile*> profiles;
  AutofillTable::FromWebDatabase(db)->GetAutofillProfiles(&profiles);
  return scoped_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<AutofillProfile*> >(
          AUTOFILL_PROFILES_RESULT,
          profiles,
          base::Bind(&AutofillWebDataBackendImpl::DestroyAutofillProfileResult,
              base::Unretained(this))));
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateAutofillEntries(
    const std::vector<autofill::AutofillEntry>& autofill_entries,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!AutofillTable::FromWebDatabase(db)
           ->UpdateAutofillEntries(autofill_entries))
    return WebDatabase::COMMIT_NOT_NEEDED;

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddCreditCard(
    const CreditCard& credit_card, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!AutofillTable::FromWebDatabase(db)->AddCreditCard(credit_card)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateCreditCard(
    const CreditCard& credit_card, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  // It is currently valid to try to update a missing profile.  We simply drop
  // the write and the caller will detect this on the next refresh.
  CreditCard* original_credit_card = NULL;
  if (!AutofillTable::FromWebDatabase(db)->GetCreditCard(credit_card.guid(),
      &original_credit_card)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  scoped_ptr<CreditCard> scoped_credit_card(original_credit_card);

  if (!AutofillTable::FromWebDatabase(db)->UpdateCreditCard(credit_card)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveCreditCard(
    const std::string& guid, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!AutofillTable::FromWebDatabase(db)->RemoveCreditCard(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetCreditCards(
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  std::vector<CreditCard*> credit_cards;
  AutofillTable::FromWebDatabase(db)->GetCreditCards(&credit_cards);
  return scoped_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<CreditCard*> >(
          AUTOFILL_CREDITCARDS_RESULT,
          credit_cards,
        base::Bind(&AutofillWebDataBackendImpl::DestroyAutofillCreditCardResult,
              base::Unretained(this))));
}

WebDatabase::State
    AutofillWebDataBackendImpl::RemoveAutofillDataModifiedBetween(
        const base::Time& delete_begin,
        const base::Time& delete_end,
        WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  std::vector<std::string> profile_guids;
  std::vector<std::string> credit_card_guids;
  if (AutofillTable::FromWebDatabase(db)->RemoveAutofillDataModifiedBetween(
          delete_begin,
          delete_end,
          &profile_guids,
          &credit_card_guids)) {
    for (std::vector<std::string>::iterator iter = profile_guids.begin();
         iter != profile_guids.end(); ++iter) {
      AutofillProfileChange change(AutofillProfileChange::REMOVE, *iter, NULL);
      FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                        db_observer_list_,
                        AutofillProfileChanged(change));
    }
    // Note: It is the caller's responsibility to post notifications for any
    // changes, e.g. by calling the Refresh() method of PersonalDataManager.
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveOriginURLsModifiedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  ScopedVector<AutofillProfile> profiles;
  if (AutofillTable::FromWebDatabase(db)->RemoveOriginURLsModifiedBetween(
          delete_begin, delete_end, &profiles)) {
    for (std::vector<AutofillProfile*>::const_iterator it = profiles.begin();
         it != profiles.end(); ++it) {
      AutofillProfileChange change(AutofillProfileChange::UPDATE,
                                   (*it)->guid(), *it);
      FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                        db_observer_list_,
                        AutofillProfileChanged(change));
    }
    // Note: It is the caller's responsibility to post notifications for any
    // changes, e.g. by calling the Refresh() method of PersonalDataManager.
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveExpiredFormElementsImpl(
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  AutofillChangeList changes;

  if (AutofillTable::FromWebDatabase(db)->RemoveExpiredFormElements(&changes)) {
    if (!changes.empty()) {
      // Post the notifications including the list of affected keys.
      // This is sent here so that work resulting from this notification
      // will be done on the DB thread, and not the UI thread.
      FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                        db_observer_list_,
                        AutofillEntriesChanged(changes));
    }
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

void AutofillWebDataBackendImpl::DestroyAutofillProfileResult(
    const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_PROFILES_RESULT);
  const WDResult<std::vector<AutofillProfile*> >* r =
      static_cast<const WDResult<std::vector<AutofillProfile*> >*>(result);
  std::vector<AutofillProfile*> profiles = r->GetValue();
  STLDeleteElements(&profiles);
}

void AutofillWebDataBackendImpl::DestroyAutofillCreditCardResult(
      const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_CREDITCARDS_RESULT);
  const WDResult<std::vector<CreditCard*> >* r =
      static_cast<const WDResult<std::vector<CreditCard*> >*>(result);

  std::vector<CreditCard*> credit_cards = r->GetValue();
  STLDeleteElements(&credit_cards);
}

}  // namespace autofill
