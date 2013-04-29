// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_service_base.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "components/webdata/common/web_database_service.h"
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

void WebDataServiceBase::WebDatabaseLoaded() {
  db_loaded_ = true;
}

void WebDataServiceBase::WebDatabaseLoadFailed(sql::InitStatus status) {
  if (!profile_error_callback_.is_null())
    profile_error_callback_.Run(status);
}

void WebDataServiceBase::ShutdownOnUIThread() {
  db_loaded_ = false;
}

void WebDataServiceBase::Init() {
  DCHECK(wdbs_.get());
  wdbs_->AddObserver(this);
  wdbs_->LoadDatabase();
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

void WebDataServiceBase::AddDBObserver(WebDatabaseObserver* observer) {
  if (!wdbs_)
    return;
  wdbs_->AddObserver(observer);
}

void WebDataServiceBase::RemoveDBObserver(WebDatabaseObserver* observer) {
  if (!wdbs_)
    return;
  wdbs_->RemoveObserver(observer);
}

WebDatabase* WebDataServiceBase::GetDatabase() {
  if (!wdbs_)
    return NULL;
  return wdbs_->GetDatabaseOnDB();
}

WebDataServiceBase::~WebDataServiceBase() {
}
