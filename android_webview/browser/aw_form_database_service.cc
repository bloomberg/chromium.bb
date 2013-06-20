// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_form_database_service.h"
#include "base/logging.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/webdata/common/webdata_constants.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util_android.h"

using content::BrowserThread;

namespace {

// Callback to handle database error. It seems chrome uses this to
// display an error dialog box only.
void DatabaseErrorCallback(sql::InitStatus status) {
  LOG(WARNING) << "initializing autocomplete database failed";
}

}  // namespace

namespace android_webview {

AwFormDatabaseService::AwFormDatabaseService(const base::FilePath path)
    : pending_query_handle_(0),
      has_form_data_(false),
      completion_(false, false) {

  web_database_ = new WebDatabaseService(path.Append(kWebDataFilename),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB));
  web_database_->AddTable(
      scoped_ptr<WebDatabaseTable>(new autofill::AutofillTable(
          l10n_util::GetDefaultLocale())));
  web_database_->LoadDatabase();

  autofill_data_ = new autofill::AutofillWebDataService(
      web_database_, base::Bind(&DatabaseErrorCallback));
  autofill_data_->Init();
}

AwFormDatabaseService::~AwFormDatabaseService() {
  CancelPendingQuery();
  Shutdown();
}

void AwFormDatabaseService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  autofill_data_->ShutdownOnUIThread();
  web_database_->ShutdownDatabase();
}

void AwFormDatabaseService::CancelPendingQuery() {
  if (pending_query_handle_) {
    if (autofill_data_.get())
      autofill_data_->CancelRequest(pending_query_handle_);
    pending_query_handle_ = 0;
  }
}

scoped_refptr<autofill::AutofillWebDataService>
AwFormDatabaseService::get_autofill_webdata_service() {
  return autofill_data_;
}

void AwFormDatabaseService::ClearFormData() {
  base::Time begin;
  base::Time end = base::Time::Max();
  autofill_data_->RemoveFormElementsAddedBetween(begin, end);
  autofill_data_->RemoveAutofillDataModifiedBetween(begin, end);
}

bool AwFormDatabaseService::HasFormData() {
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&AwFormDatabaseService::HasFormDataImpl,
                 base::Unretained(this)));
  completion_.Wait();
  return has_form_data_;
}

void AwFormDatabaseService::HasFormDataImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  pending_query_handle_ = autofill_data_->HasFormElements(this);
}


void AwFormDatabaseService::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    const WDTypedResult* result) {

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK_EQ(pending_query_handle_, h);
  pending_query_handle_ = 0;
  has_form_data_ = false;

  if (result) {
    DCHECK_EQ(AUTOFILL_VALUE_RESULT, result->GetType());
    const WDResult<bool>* autofill_result =
        static_cast<const WDResult<bool>*>(result);
    has_form_data_ = autofill_result->GetValue();
  }
  completion_.Signal();
}

}  // namespace android_webview
