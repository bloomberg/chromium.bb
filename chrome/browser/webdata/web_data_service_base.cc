// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/api/webdata/web_data_service_base.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "chrome/browser/webdata/web_database_service.h"
#ifdef DEBUG
#include "content/public/browser/browser_thread.h"
#endif

////////////////////////////////////////////////////////////////////////////////
//
// WebDataServiceBase implementation.
//
////////////////////////////////////////////////////////////////////////////////

using base::Bind;
using base::Time;
using content::BrowserThread;

WebDataServiceBase::WebDataServiceBase(scoped_refptr<WebDatabaseService> wdbs,
                                       const ProfileErrorCallback& callback)
    : wdbs_(wdbs),
      db_loaded_(false),
      profile_error_callback_(callback) {
  // WebDataService requires DB thread if instantiated.
  // Set WebDataServiceFactory::GetInstance()->SetTestingFactory(&profile, NULL)
  // if you do not want to instantiate WebDataService in your test.
  DCHECK(BrowserThread::IsWellKnownThread(BrowserThread::DB));
}

void WebDataServiceBase::ShutdownOnUIThread() {
  db_loaded_ = false;
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&WebDataServiceBase::ShutdownOnDBThread, this));
}

void WebDataServiceBase::Init() {
  DCHECK(wdbs_.get());
  wdbs_->LoadDatabase(Bind(&WebDataServiceBase::DatabaseInitOnDB, this));
}

void WebDataServiceBase::UnloadDatabase() {
  if (!wdbs_)
    return;
  wdbs_->UnloadDatabase();
}

void WebDataServiceBase::ShutdownDatabase() {
  if (!wdbs_)
    return;
  wdbs_->ShutdownDatabase();
}

void WebDataServiceBase::CancelRequest(Handle h) {
  if (!wdbs_)
    return;
  wdbs_->CancelRequest(h);
}

content::NotificationSource WebDataServiceBase::GetNotificationSource() {
  return content::Source<WebDataServiceBase>(this);
}

bool WebDataServiceBase::IsDatabaseLoaded() {
  return db_loaded_;
}

WebDatabase* WebDataServiceBase::GetDatabase() {
  if (!wdbs_)
    return NULL;
  return wdbs_->GetDatabaseOnDB();
}

base::SupportsUserData* WebDataServiceBase::GetDBUserData() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!db_thread_user_data_)
    db_thread_user_data_.reset(new SupportsUserDataAggregatable());
  return db_thread_user_data_.get();
}

WebDataServiceBase::~WebDataServiceBase() {
  DCHECK(!db_thread_user_data_.get()) << "Forgot to call ShutdownOnUIThread?";
}

void WebDataServiceBase::ShutdownOnDBThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  db_thread_user_data_.reset();
}

void WebDataServiceBase::NotifyDatabaseLoadedOnUIThread() {}

void WebDataServiceBase::DBInitFailed(sql::InitStatus sql_status) {
  if (!profile_error_callback_.is_null())
    profile_error_callback_.Run(sql_status);
}

void WebDataServiceBase::DBInitSucceeded() {
  db_loaded_ = true;
  NotifyDatabaseLoadedOnUIThread();
}

// Executed on DB thread.
void WebDataServiceBase::DatabaseInitOnDB(sql::InitStatus status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (status == sql::INIT_OK) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WebDataServiceBase::DBInitSucceeded, this));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WebDataServiceBase::DBInitFailed, this, status));
  }
}
