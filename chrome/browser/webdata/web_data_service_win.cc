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
  ScheduleDBTask(FROM_HERE,
                 Bind(&WebDataService::AddIE7LoginImpl, this, info));
}

void WebDataService::RemoveIE7Login(const IE7PasswordInfo& info) {
  ScheduleDBTask(FROM_HERE,
                 Bind(&WebDataService::RemoveIE7LoginImpl, this, info));
}

WebDataService::Handle WebDataService::GetIE7Login(
    const IE7PasswordInfo& info,
    WebDataServiceConsumer* consumer) {
  return ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetIE7LoginImpl, this, info), consumer);
}

void WebDataService::AddIE7LoginImpl(const IE7PasswordInfo& info) {
  if (db_->GetLoginsTable()->AddIE7Login(info))
      ScheduleCommit();
}

void WebDataService::RemoveIE7LoginImpl(const IE7PasswordInfo& info) {
  if (db_->GetLoginsTable()->RemoveIE7Login(info))
    ScheduleCommit();
}

scoped_ptr<WDTypedResult> WebDataService::GetIE7LoginImpl(
    const IE7PasswordInfo& info) {
    IE7PasswordInfo result;
    db_->GetLoginsTable()->GetIE7Login(info, &result);
    return scoped_ptr<WDTypedResult>(
        new WDResult<IE7PasswordInfo>(PASSWORD_IE7_RESULT, result));
}
