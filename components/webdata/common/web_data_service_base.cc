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
      profile_error_callback_(callback) {
  // WebDataService requires DB thread if instantiated.
  // Set WebDataServiceFactory::GetInstance()->SetTestingFactory(&profile, NULL)
  // if you do not want to instantiate WebDataService in your test.
  DCHECK(BrowserThread::IsWellKnownThread(BrowserThread::DB));
}

void WebDataServiceBase::ShutdownOnUIThread() {
}

void WebDataServiceBase::Init() {
  DCHECK(wdbs_.get());
  wdbs_->RegisterDBErrorCallback(profile_error_callback_);
  wdbs_->LoadDatabase();
}

void WebDataServiceBase::UnloadDatabase() {
  if (!wdbs_.get())
    return;
  wdbs_->UnloadDatabase();
}

void WebDataServiceBase::ShutdownDatabase() {
  if (!wdbs_.get())
    return;
  wdbs_->ShutdownDatabase();
}

void WebDataServiceBase::CancelRequest(Handle h) {
  if (!wdbs_.get())
    return;
  wdbs_->CancelRequest(h);
}

bool WebDataServiceBase::IsDatabaseLoaded() {
  if (!wdbs_.get())
    return false;
  return wdbs_->db_loaded();
}

void WebDataServiceBase::RegisterDBLoadedCallback(
    const base::Callback<void(void)>& callback) {
  if (!wdbs_.get())
    return;
  wdbs_->RegisterDBLoadedCallback(callback);
}

WebDatabase* WebDataServiceBase::GetDatabase() {
  if (!wdbs_.get())
    return NULL;
  return wdbs_->GetDatabaseOnDB();
}

WebDataServiceBase::~WebDataServiceBase() {
}
