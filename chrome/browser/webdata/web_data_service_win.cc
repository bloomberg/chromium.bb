// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/logins_table.h"
#include "chrome/browser/webdata/web_data_service.h"

#include "base/bind.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/browser/webdata/web_database.h"

using base::Bind;

void WebDataService::AddIE7Login(const IE7PasswordInfo& info) {
  GenericRequest<IE7PasswordInfo>* request =
      new GenericRequest<IE7PasswordInfo>(this, NULL, &request_manager_, info);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::AddIE7LoginImpl, this, request));
}

void WebDataService::RemoveIE7Login(const IE7PasswordInfo& info) {
  GenericRequest<IE7PasswordInfo>* request =
      new GenericRequest<IE7PasswordInfo>(this, NULL, &request_manager_, info);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveIE7LoginImpl, this, request));
}

WebDataService::Handle WebDataService::GetIE7Login(
    const IE7PasswordInfo& info,
    WebDataServiceConsumer* consumer) {
  GenericRequest<IE7PasswordInfo>* request =
      new GenericRequest<IE7PasswordInfo>(this, consumer, &request_manager_,
                                          info);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetIE7LoginImpl, this, request));
  return request->GetHandle();
}

void WebDataService::AddIE7LoginImpl(GenericRequest<IE7PasswordInfo>* request) {
  if (db_ && !request->IsCancelled()) {
    if (db_->GetLoginsTable()->AddIE7Login(request->arg()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveIE7LoginImpl(
    GenericRequest<IE7PasswordInfo>* request) {
  if (db_ && !request->IsCancelled()) {
    if (db_->GetLoginsTable()->RemoveIE7Login(request->arg()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::GetIE7LoginImpl(
    GenericRequest<IE7PasswordInfo>* request) {
  if (db_ && !request->IsCancelled()) {
    IE7PasswordInfo result;
    db_->GetLoginsTable()->GetIE7Login(request->arg(), &result);
    request->SetResult(
        new WDResult<IE7PasswordInfo>(PASSWORD_IE7_RESULT, result));
  }
  request->RequestComplete();
}
