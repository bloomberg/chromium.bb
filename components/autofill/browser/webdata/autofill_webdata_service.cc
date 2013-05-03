// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/webdata/autofill_webdata_service.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/autofill/browser/autofill_country.h"
#include "components/autofill/browser/autofill_profile.h"
#include "components/autofill/browser/credit_card.h"
#include "components/autofill/browser/webdata/autofill_change.h"
#include "components/autofill/browser/webdata/autofill_entry.h"
#include "components/autofill/browser/webdata/autofill_table.h"
#include "components/autofill/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/common/form_field_data.h"
#include "components/webdata/common/web_database_service.h"

using base::Bind;
using base::Time;
using content::BrowserThread;

namespace autofill {

// static
void AutofillWebDataService::NotifyOfMultipleAutofillChanges(
    AutofillWebDataService* web_data_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  if (!web_data_service)
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      Bind(&AutofillWebDataService::NotifyAutofillMultipleChangedOnUIThread,
           make_scoped_refptr(web_data_service)));
}

AutofillWebDataService::AutofillWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    const ProfileErrorCallback& callback)
    : WebDataServiceBase(wdbs, callback),
      autofill_backend_(new AutofillWebDataBackend()) {
}

AutofillWebDataService::AutofillWebDataService()
    : WebDataServiceBase(NULL,
                         WebDataServiceBase::ProfileErrorCallback()),
      autofill_backend_(new AutofillWebDataBackend()) {
}

void AutofillWebDataService::ShutdownOnUIThread() {
  BrowserThread::PostTask(
    BrowserThread::DB, FROM_HERE,
    base::Bind(&AutofillWebDataService::ShutdownOnDBThread, this));
  WebDataServiceBase::ShutdownOnUIThread();
}

void AutofillWebDataService::AddFormFields(
    const std::vector<FormFieldData>& fields) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackend::AddFormElements,
           autofill_backend_, fields));
}

WebDataServiceBase::Handle AutofillWebDataService::GetFormValuesForElementName(
    const base::string16& name, const base::string16& prefix, int limit,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&AutofillWebDataBackend::GetFormValuesForElementName,
           autofill_backend_, name, prefix, limit), consumer);
}

void AutofillWebDataService::RemoveFormElementsAddedBetween(
    const Time& delete_begin, const Time& delete_end) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackend::RemoveFormElementsAddedBetween,
           autofill_backend_, delete_begin, delete_end));
}

void AutofillWebDataService::RemoveExpiredFormElements() {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackend::RemoveExpiredFormElements,
           autofill_backend_));
}

void AutofillWebDataService::RemoveFormValueForElementName(
    const base::string16& name, const base::string16& value) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackend::RemoveFormValueForElementName,
           autofill_backend_, name, value));
}

void AutofillWebDataService::AddAutofillProfile(
    const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackend::AddAutofillProfile,
           autofill_backend_, profile));
}

void AutofillWebDataService::UpdateAutofillProfile(
    const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackend::UpdateAutofillProfile,
           autofill_backend_, profile));
}

void AutofillWebDataService::RemoveAutofillProfile(
    const std::string& guid) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&AutofillWebDataBackend::RemoveAutofillProfile,
           autofill_backend_, guid));
}

WebDataServiceBase::Handle AutofillWebDataService::GetAutofillProfiles(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&AutofillWebDataBackend::GetAutofillProfiles, autofill_backend_),
      consumer);
}

void AutofillWebDataService::AddCreditCard(const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackend::AddCreditCard,
           autofill_backend_, credit_card));
}

void AutofillWebDataService::UpdateCreditCard(
    const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackend::UpdateCreditCard,
           autofill_backend_, credit_card));
}

void AutofillWebDataService::RemoveCreditCard(const std::string& guid) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackend::RemoveCreditCard, autofill_backend_, guid));
}

WebDataServiceBase::Handle AutofillWebDataService::GetCreditCards(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&AutofillWebDataBackend::GetCreditCards, autofill_backend_),
      consumer);
}

void AutofillWebDataService::RemoveAutofillDataModifiedBetween(
    const Time& delete_begin,
    const Time& delete_end) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&AutofillWebDataBackend::RemoveAutofillDataModifiedBetween,
           autofill_backend_, delete_begin, delete_end));
}

void AutofillWebDataService::AddObserver(
    AutofillWebDataServiceObserverOnDBThread* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (autofill_backend_)
    autofill_backend_->AddObserver(observer);
}

void AutofillWebDataService::RemoveObserver(
    AutofillWebDataServiceObserverOnDBThread* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (autofill_backend_)
    autofill_backend_->RemoveObserver(observer);
}

void AutofillWebDataService::AddObserver(
    AutofillWebDataServiceObserverOnUIThread* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ui_observer_list_.AddObserver(observer);
}

void AutofillWebDataService::RemoveObserver(
    AutofillWebDataServiceObserverOnUIThread* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ui_observer_list_.RemoveObserver(observer);
}

base::SupportsUserData* AutofillWebDataService::GetDBUserData() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!db_thread_user_data_)
    db_thread_user_data_.reset(new SupportsUserDataAggregatable());
  return db_thread_user_data_.get();
}

void AutofillWebDataService::ShutdownOnDBThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  db_thread_user_data_.reset();
}

AutofillWebDataService::~AutofillWebDataService() {
  DCHECK(!db_thread_user_data_.get()) << "Forgot to call ShutdownOnUIThread?";
}

void AutofillWebDataService::NotifyAutofillMultipleChangedOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnUIThread,
                    ui_observer_list_,
                    AutofillMultipleChanged());
}

}  // namespace autofill
