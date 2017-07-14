// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_form_database_service.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/webdata/common/webdata_constants.h"
#include "content/public/browser/browser_thread.h"

using base::WaitableEvent;
using content::BrowserThread;

namespace {

// Callback to handle database error. It seems chrome uses this to
// display an error dialog box only.
void DatabaseErrorCallback(sql::InitStatus init_status,
                           const std::string& diagnostics) {
  LOG(WARNING) << "initializing autocomplete database failed";
}

}  // namespace

namespace android_webview {

AwFormDatabaseService::AwFormDatabaseService(const base::FilePath path) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_database_ = new WebDatabaseService(
      path.Append(kWebDataFilename),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::DB));
  web_database_->AddTable(base::WrapUnique(new autofill::AutofillTable));
  web_database_->LoadDatabase();

  autofill_data_ = new autofill::AutofillWebDataService(
      web_database_, BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::DB),
      base::Bind(&DatabaseErrorCallback));
  autofill_data_->Init();
}

AwFormDatabaseService::~AwFormDatabaseService() {
  Shutdown();
}

void AwFormDatabaseService::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(result_map_.empty());
  // TODO(sgurun) we don't run into this logic right now,
  // but if we do, then we need to implement cancellation
  // of pending queries.
  autofill_data_->ShutdownOnUIThread();
  web_database_->ShutdownDatabase();
}

scoped_refptr<autofill::AutofillWebDataService>
AwFormDatabaseService::get_autofill_webdata_service() {
  return autofill_data_;
}

void AwFormDatabaseService::ClearFormData() {
  BrowserThread::PostTask(
      BrowserThread::DB,
      FROM_HERE,
      base::Bind(&AwFormDatabaseService::ClearFormDataImpl,
                 base::Unretained(this)));
}

void AwFormDatabaseService::ClearFormDataImpl() {
  base::Time begin;
  base::Time end = base::Time::Max();
  autofill_data_->RemoveFormElementsAddedBetween(begin, end);
  autofill_data_->RemoveAutofillDataModifiedBetween(begin, end);
}

bool AwFormDatabaseService::HasFormData() {
  WaitableEvent completion(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool result = false;
  BrowserThread::PostTask(
      BrowserThread::DB,
      FROM_HERE,
      base::Bind(&AwFormDatabaseService::HasFormDataImpl,
                 base::Unretained(this),
                 &completion,
                 &result));
  {
    base::ThreadRestrictions::ScopedAllowWait wait;
    completion.Wait();
  }
  return result;
}

void AwFormDatabaseService::HasFormDataImpl(
    WaitableEvent* completion,
    bool* result) {
  WebDataServiceBase::Handle pending_query_handle =
      autofill_data_->GetCountOfValuesContainedBetween(
          base::Time(), base::Time::Max(), this);
  PendingQuery query;
  query.result = result;
  query.completion = completion;
  result_map_[pending_query_handle] = query;
}

void AwFormDatabaseService::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  bool has_form_data = false;
  if (result) {
    DCHECK_EQ(AUTOFILL_VALUE_RESULT, result->GetType());
    const WDResult<int>* autofill_result =
        static_cast<const WDResult<int>*>(result.get());
    has_form_data = autofill_result->GetValue() > 0;
  }
  QueryMap::const_iterator it = result_map_.find(h);
  if (it == result_map_.end()) {
    LOG(WARNING) << "Received unexpected callback from web data service";
    return;
  }

  WaitableEvent* completion = it->second.completion;
  *(it->second.result) = has_form_data;
  result_map_.erase(h);

  completion->Signal();
}

}  // namespace android_webview
