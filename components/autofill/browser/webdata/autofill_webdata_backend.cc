// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/webdata/autofill_webdata_backend.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/autofill/browser/autofill_country.h"
#include "components/autofill/browser/autofill_profile.h"
#include "components/autofill/browser/credit_card.h"
#include "components/autofill/browser/webdata/autofill_change.h"
#include "components/autofill/browser/webdata/autofill_entry.h"
#include "components/autofill/browser/webdata/autofill_table.h"
#include "components/autofill/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/common/form_field_data.h"

using base::Bind;
using base::Time;
using content::BrowserThread;

namespace autofill {

AutofillWebDataBackend::AutofillWebDataBackend() {
}

void AutofillWebDataBackend::AddObserver(
    AutofillWebDataServiceObserverOnDBThread* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  db_observer_list_.AddObserver(observer);
}

void AutofillWebDataBackend::RemoveObserver(
    AutofillWebDataServiceObserverOnDBThread* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  db_observer_list_.RemoveObserver(observer);
}

AutofillWebDataBackend::~AutofillWebDataBackend() {
}

WebDatabase::State AutofillWebDataBackend::AddFormElements(
    const std::vector<FormFieldData>& fields, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
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
AutofillWebDataBackend::GetFormValuesForElementName(
    const base::string16& name, const base::string16& prefix, int limit,
    WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  std::vector<base::string16> values;
  AutofillTable::FromWebDatabase(db)->GetFormValuesForElementName(
      name, prefix, &values, limit);
  return scoped_ptr<WDTypedResult>(
      new WDResult<std::vector<base::string16> >(AUTOFILL_VALUE_RESULT,
                                                 values));
}

WebDatabase::State AutofillWebDataBackend::RemoveFormElementsAddedBetween(
    const base::Time& delete_begin, const base::Time& delete_end,
    WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
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

WebDatabase::State AutofillWebDataBackend::RemoveExpiredFormElements(
    WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
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

WebDatabase::State AutofillWebDataBackend::RemoveFormValueForElementName(
    const base::string16& name, const base::string16& value, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

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

WebDatabase::State AutofillWebDataBackend::AddAutofillProfile(
    const AutofillProfile& profile, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
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

WebDatabase::State AutofillWebDataBackend::UpdateAutofillProfile(
    const AutofillProfile& profile, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  // Only perform the update if the profile exists.  It is currently
  // valid to try to update a missing profile.  We simply drop the write and
  // the caller will detect this on the next refresh.
  AutofillProfile* original_profile = NULL;
  if (!AutofillTable::FromWebDatabase(db)->GetAutofillProfile(profile.guid(),
      &original_profile)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  scoped_ptr<AutofillProfile> scoped_profile(original_profile);

  if (!AutofillTable::FromWebDatabase(db)->UpdateAutofillProfileMulti(
        profile)) {
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

WebDatabase::State AutofillWebDataBackend::RemoveAutofillProfile(
    const std::string& guid, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
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

scoped_ptr<WDTypedResult> AutofillWebDataBackend::GetAutofillProfiles(
    WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  std::vector<AutofillProfile*> profiles;
  AutofillTable::FromWebDatabase(db)->GetAutofillProfiles(&profiles);
  return scoped_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<AutofillProfile*> >(
          AUTOFILL_PROFILES_RESULT,
          profiles,
          base::Bind(&AutofillWebDataBackend::DestroyAutofillProfileResult,
              base::Unretained(this))));
}

WebDatabase::State AutofillWebDataBackend::AddCreditCard(
    const CreditCard& credit_card, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!AutofillTable::FromWebDatabase(db)->AddCreditCard(credit_card)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackend::UpdateCreditCard(
    const CreditCard& credit_card, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
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

WebDatabase::State AutofillWebDataBackend::RemoveCreditCard(
    const std::string& guid, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!AutofillTable::FromWebDatabase(db)->RemoveCreditCard(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> AutofillWebDataBackend::GetCreditCards(
    WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  std::vector<CreditCard*> credit_cards;
  AutofillTable::FromWebDatabase(db)->GetCreditCards(&credit_cards);
  return scoped_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<CreditCard*> >(
          AUTOFILL_CREDITCARDS_RESULT,
          credit_cards,
        base::Bind(&AutofillWebDataBackend::DestroyAutofillCreditCardResult,
              base::Unretained(this))));
}

WebDatabase::State
    AutofillWebDataBackend::RemoveAutofillDataModifiedBetween(
        const base::Time& delete_begin,
        const base::Time& delete_end,
        WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
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

void AutofillWebDataBackend::DestroyAutofillProfileResult(
    const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_PROFILES_RESULT);
  const WDResult<std::vector<AutofillProfile*> >* r =
      static_cast<const WDResult<std::vector<AutofillProfile*> >*>(result);
  std::vector<AutofillProfile*> profiles = r->GetValue();
  STLDeleteElements(&profiles);
}

void AutofillWebDataBackend::DestroyAutofillCreditCardResult(
      const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_CREDITCARDS_RESULT);
  const WDResult<std::vector<CreditCard*> >* r =
      static_cast<const WDResult<std::vector<CreditCard*> >*>(result);

  std::vector<CreditCard*> credit_cards = r->GetValue();
  STLDeleteElements(&credit_cards);
}

}  // namespace autofill
