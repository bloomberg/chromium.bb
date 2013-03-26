// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/autofill_web_data_service_impl.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/web_database_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "components/autofill/browser/autofill_country.h"
#include "components/autofill/browser/autofill_profile.h"
#include "components/autofill/browser/credit_card.h"
#include "components/autofill/common/form_field_data.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

using base::Bind;
using base::Time;
using content::BrowserThread;

namespace {

// A task used by AutofillWebDataService (for Sync mainly) to inform the
// PersonalDataManager living on the UI thread that it needs to refresh.
void NotifyOfMultipleAutofillChangesTask(
    const scoped_refptr<AutofillWebDataService>& web_data_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_MULTIPLE_CHANGED,
      content::Source<AutofillWebDataService>(web_data_service.get()),
      content::NotificationService::NoDetails());
}
}

// static
void AutofillWebDataService::NotifyOfMultipleAutofillChanges(
    AutofillWebDataService* web_data_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  if (!web_data_service)
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      Bind(&NotifyOfMultipleAutofillChangesTask,
           make_scoped_refptr(web_data_service)));
}

AutofillWebDataService::AutofillWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    const ProfileErrorCallback& callback)
    : WebDataServiceBase(wdbs, callback)  {
}

AutofillWebDataService::AutofillWebDataService()
    : WebDataServiceBase(NULL,
                         WebDataServiceBase::ProfileErrorCallback()) {
}

AutofillWebDataServiceImpl::AutofillWebDataServiceImpl(
    scoped_refptr<WebDatabaseService> wdbs,
    const ProfileErrorCallback& callback)
    : AutofillWebDataService(wdbs, callback) {
}

content::NotificationSource
AutofillWebDataServiceImpl::GetNotificationSource() {
  return content::Source<AutofillWebDataService>(this);
}

void AutofillWebDataServiceImpl::AddFormFields(
    const std::vector<FormFieldData>& fields) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::AddFormElementsImpl, this, fields));
}

WebDataServiceBase::Handle
AutofillWebDataServiceImpl::GetFormValuesForElementName(
    const string16& name, const string16& prefix, int limit,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::GetFormValuesForElementNameImpl,
           this, name, prefix, limit), consumer);
}

void AutofillWebDataServiceImpl::RemoveFormElementsAddedBetween(
    const Time& delete_begin, const Time& delete_end) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::RemoveFormElementsAddedBetweenImpl,
           this, delete_begin, delete_end));
}

void AutofillWebDataServiceImpl::RemoveExpiredFormElements() {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::RemoveExpiredFormElementsImpl, this));
}

void AutofillWebDataServiceImpl::RemoveFormValueForElementName(
    const string16& name, const string16& value) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::RemoveFormValueForElementNameImpl,
           this, name, value));
}

void AutofillWebDataServiceImpl::AddAutofillProfile(
    const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::AddAutofillProfileImpl, this, profile));
}

void AutofillWebDataServiceImpl::UpdateAutofillProfile(
    const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::UpdateAutofillProfileImpl,
           this, profile));
}

void AutofillWebDataServiceImpl::RemoveAutofillProfile(
    const std::string& guid) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::RemoveAutofillProfileImpl, this, guid));
}

WebDataServiceBase::Handle AutofillWebDataServiceImpl::GetAutofillProfiles(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::GetAutofillProfilesImpl, this),
      consumer);
}

void AutofillWebDataServiceImpl::AddCreditCard(const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::AddCreditCardImpl, this, credit_card));
}

void AutofillWebDataServiceImpl::UpdateCreditCard(
    const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::UpdateCreditCardImpl, this,
           credit_card));
}

void AutofillWebDataServiceImpl::RemoveCreditCard(const std::string& guid) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::RemoveCreditCardImpl, this, guid));
}

AutofillWebDataServiceImpl::Handle AutofillWebDataServiceImpl::GetCreditCards(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&AutofillWebDataServiceImpl::GetCreditCardsImpl, this), consumer);
}

void AutofillWebDataServiceImpl::RemoveAutofillDataModifiedBetween(
    const Time& delete_begin, const Time& delete_end) {
  wdbs_->ScheduleDBTask(FROM_HERE, Bind(
&AutofillWebDataServiceImpl::RemoveAutofillDataModifiedBetweenImpl,
      this, delete_begin, delete_end));
}

AutofillWebDataServiceImpl::~AutofillWebDataServiceImpl() {
}

////////////////////////////////////////////////////////////////////////////////
//
// Autofill implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDatabase::State AutofillWebDataServiceImpl::AddFormElementsImpl(
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
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
      content::Source<AutofillWebDataService>(this),
      content::Details<AutofillChangeList>(&changes));

  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult>
AutofillWebDataServiceImpl::GetFormValuesForElementNameImpl(
    const string16& name, const string16& prefix, int limit, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  std::vector<string16> values;
  AutofillTable::FromWebDatabase(db)->GetFormValuesForElementName(
      name, prefix, &values, limit);
  return scoped_ptr<WDTypedResult>(
      new WDResult<std::vector<string16> >(AUTOFILL_VALUE_RESULT, values));
}

WebDatabase::State
AutofillWebDataServiceImpl::RemoveFormElementsAddedBetweenImpl(
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
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
          content::Source<AutofillWebDataService>(this),
          content::Details<AutofillChangeList>(&changes));
    }
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataServiceImpl::RemoveExpiredFormElementsImpl(
    WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  AutofillChangeList changes;

  if (AutofillTable::FromWebDatabase(db)->RemoveExpiredFormElements(&changes)) {
    if (!changes.empty()) {
      // Post the notifications including the list of affected keys.
      // This is sent here so that work resulting from this notification
      // will be done on the DB thread, and not the UI thread.
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
          content::Source<AutofillWebDataService>(this),
          content::Details<AutofillChangeList>(&changes));
    }
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State
AutofillWebDataServiceImpl::RemoveFormValueForElementNameImpl(
    const string16& name, const string16& value, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  if (AutofillTable::FromWebDatabase(db)->RemoveFormElement(name, value)) {
    AutofillChangeList changes;
    changes.push_back(AutofillChange(AutofillChange::REMOVE,
                                     AutofillKey(name, value)));

    // Post the notifications including the list of affected keys.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
        content::Source<AutofillWebDataService>(this),
        content::Details<AutofillChangeList>(&changes));

    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataServiceImpl::AddAutofillProfileImpl(
    const AutofillProfile& profile, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!AutofillTable::FromWebDatabase(db)->AddAutofillProfile(profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::ADD,
                               profile.guid(), &profile);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
      content::Source<AutofillWebDataService>(this),
      content::Details<AutofillProfileChange>(&change));

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataServiceImpl::UpdateAutofillProfileImpl(
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
  AutofillProfileChange change(AutofillProfileChange::UPDATE,
                               profile.guid(), &profile);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
      content::Source<AutofillWebDataService>(this),
      content::Details<AutofillProfileChange>(&change));

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataServiceImpl::RemoveAutofillProfileImpl(
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
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
      content::Source<AutofillWebDataService>(this),
      content::Details<AutofillProfileChange>(&change));

  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> AutofillWebDataServiceImpl::GetAutofillProfilesImpl(
    WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  std::vector<AutofillProfile*> profiles;
  AutofillTable::FromWebDatabase(db)->GetAutofillProfiles(&profiles);
  return scoped_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<AutofillProfile*> >(
          AUTOFILL_PROFILES_RESULT,
          profiles,
          base::Bind(&AutofillWebDataServiceImpl::DestroyAutofillProfileResult,
              base::Unretained(this))));
}

WebDatabase::State AutofillWebDataServiceImpl::AddCreditCardImpl(
    const CreditCard& credit_card, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!AutofillTable::FromWebDatabase(db)->AddCreditCard(credit_card)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataServiceImpl::UpdateCreditCardImpl(
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

WebDatabase::State AutofillWebDataServiceImpl::RemoveCreditCardImpl(
    const std::string& guid, WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!AutofillTable::FromWebDatabase(db)->RemoveCreditCard(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> AutofillWebDataServiceImpl::GetCreditCardsImpl(
    WebDatabase* db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  std::vector<CreditCard*> credit_cards;
  AutofillTable::FromWebDatabase(db)->GetCreditCards(&credit_cards);
  return scoped_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<CreditCard*> >(
          AUTOFILL_CREDITCARDS_RESULT,
          credit_cards,
        base::Bind(&AutofillWebDataServiceImpl::DestroyAutofillCreditCardResult,
              base::Unretained(this))));
}

WebDatabase::State
AutofillWebDataServiceImpl::RemoveAutofillDataModifiedBetweenImpl(
        const base::Time& delete_begin, const base::Time& delete_end,
        WebDatabase* db) {
  std::vector<std::string> profile_guids;
  std::vector<std::string> credit_card_guids;
  if (AutofillTable::FromWebDatabase(db)->
      RemoveAutofillDataModifiedBetween(
          delete_begin,
          delete_end,
          &profile_guids,
          &credit_card_guids)) {
    for (std::vector<std::string>::iterator iter = profile_guids.begin();
         iter != profile_guids.end(); ++iter) {
      AutofillProfileChange change(AutofillProfileChange::REMOVE, *iter,
                                   NULL);
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
          content::Source<AutofillWebDataService>(this),
          content::Details<AutofillProfileChange>(&change));
    }
    // Note: It is the caller's responsibility to post notifications for any
    // changes, e.g. by calling the Refresh() method of PersonalDataManager.
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

void AutofillWebDataServiceImpl::DestroyAutofillProfileResult(
    const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_PROFILES_RESULT);
  const WDResult<std::vector<AutofillProfile*> >* r =
      static_cast<const WDResult<std::vector<AutofillProfile*> >*>(result);
  std::vector<AutofillProfile*> profiles = r->GetValue();
  STLDeleteElements(&profiles);
}

void AutofillWebDataServiceImpl::DestroyAutofillCreditCardResult(
      const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_CREDITCARDS_RESULT);
  const WDResult<std::vector<CreditCard*> >* r =
      static_cast<const WDResult<std::vector<CreditCard*> >*>(result);

  std::vector<CreditCard*> credit_cards = r->GetValue();
  STLDeleteElements(&credit_cards);
}
