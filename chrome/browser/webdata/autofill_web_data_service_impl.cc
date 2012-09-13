// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/autofill_web_data_service_impl.h"

#include "base/logging.h"
#include "chrome/browser/webdata/web_data_service.h"

AutofillWebDataServiceImpl::AutofillWebDataServiceImpl(
    scoped_refptr<WebDataService> service)
    : service_(service) {
  DCHECK(service.get());
}

AutofillWebDataServiceImpl::~AutofillWebDataServiceImpl() {
}

void AutofillWebDataServiceImpl::AddFormFields(
    const std::vector<webkit::forms::FormField>& fields) {
  service_->AddFormFields(fields);
}

WebDataServiceBase::Handle
AutofillWebDataServiceImpl::GetFormValuesForElementName(
    const string16& name,
    const string16& prefix,
    int limit,
    WebDataServiceConsumer* consumer) {
  return service_->GetFormValuesForElementName(name, prefix, limit, consumer);
}

void AutofillWebDataServiceImpl::RemoveExpiredFormElements() {
  service_->RemoveExpiredFormElements();
}

void AutofillWebDataServiceImpl::RemoveFormValueForElementName(
    const string16& name, const string16& value) {
  service_->RemoveFormValueForElementName(name, value);
}

void AutofillWebDataServiceImpl::AddAutofillProfile(
    const AutofillProfile& profile) {
  service_->AddAutofillProfile(profile);
}

void AutofillWebDataServiceImpl::UpdateAutofillProfile(
    const AutofillProfile& profile) {
  service_->UpdateAutofillProfile(profile);
}

void AutofillWebDataServiceImpl::RemoveAutofillProfile(
    const std::string& guid) {
  service_->RemoveAutofillProfile(guid);
}

WebDataServiceBase::Handle AutofillWebDataServiceImpl::GetAutofillProfiles(
    WebDataServiceConsumer* consumer) {
  return service_->GetAutofillProfiles(consumer);
}

void AutofillWebDataServiceImpl::EmptyMigrationTrash(bool notify_sync) {
  service_->EmptyMigrationTrash(notify_sync);
}

void AutofillWebDataServiceImpl::AddCreditCard(const CreditCard& credit_card) {
  service_->AddCreditCard(credit_card);
}

void AutofillWebDataServiceImpl::UpdateCreditCard(
    const CreditCard& credit_card) {
  service_->UpdateCreditCard(credit_card);
}

void AutofillWebDataServiceImpl::RemoveCreditCard(const std::string& guid) {
  service_->RemoveCreditCard(guid);
}

WebDataServiceBase::Handle
AutofillWebDataServiceImpl::GetCreditCards(WebDataServiceConsumer* consumer) {
  return service_->GetCreditCards(consumer);
}

void AutofillWebDataServiceImpl::CancelRequest(Handle h) {
  service_->CancelRequest(h);
}

content::NotificationSource
AutofillWebDataServiceImpl::GetNotificationSource() {
  return service_->GetNotificationSource();
}
