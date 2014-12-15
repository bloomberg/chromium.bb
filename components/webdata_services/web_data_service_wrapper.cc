// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata_services/web_data_service_wrapper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop_proxy.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/password_manager/core/browser/webdata/logins_table.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/signin/core/browser/webdata/token_web_data.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata/common/webdata_constants.h"

#if defined(OS_WIN)
#include "components/password_manager/core/browser/webdata/password_web_data_service_win.h"
#endif

WebDataServiceWrapper::WebDataServiceWrapper() {
}

WebDataServiceWrapper::WebDataServiceWrapper(
    const base::FilePath& profile_path,
    const std::string& application_locale,
    const scoped_refptr<base::MessageLoopProxy>& ui_thread,
    const scoped_refptr<base::MessageLoopProxy>& db_thread,
    ShowErrorCallback show_error_callback) {
  base::FilePath path = profile_path.Append(kWebDataFilename);
  web_database_ = new WebDatabaseService(path, ui_thread, db_thread);

  // All tables objects that participate in managing the database must
  // be added here.
  web_database_->AddTable(scoped_ptr<WebDatabaseTable>(
      new autofill::AutofillTable(application_locale)));
  web_database_->AddTable(scoped_ptr<WebDatabaseTable>(new KeywordTable()));
  // TODO(mdm): We only really need the LoginsTable on Windows for IE7 password
  // access, but for now, we still create it on all platforms since it deletes
  // the old logins table. We can remove this after a while, e.g. in M22 or so.
  web_database_->AddTable(scoped_ptr<WebDatabaseTable>(new LoginsTable()));
  web_database_->AddTable(
      scoped_ptr<WebDatabaseTable>(new TokenServiceTable()));
  web_database_->LoadDatabase();

  autofill_web_data_ = new autofill::AutofillWebDataService(
      web_database_, ui_thread, db_thread,
      base::Bind(show_error_callback, ERROR_LOADING_AUTOFILL));
  autofill_web_data_->Init();

  keyword_web_data_ = new KeywordWebDataService(
      web_database_, ui_thread,
      base::Bind(show_error_callback, ERROR_LOADING_KEYWORD));
  keyword_web_data_->Init();

  token_web_data_ = new TokenWebData(
      web_database_, ui_thread, db_thread,
      base::Bind(show_error_callback, ERROR_LOADING_TOKEN));
  token_web_data_->Init();

#if defined(OS_WIN)
  password_web_data_ = new PasswordWebDataService(
      web_database_, ui_thread,
      base::Bind(show_error_callback, ERROR_LOADING_PASSWORD));
  password_web_data_->Init();
#endif
}

WebDataServiceWrapper::~WebDataServiceWrapper() {
}

void WebDataServiceWrapper::Shutdown() {
  autofill_web_data_->ShutdownOnUIThread();
  keyword_web_data_->ShutdownOnUIThread();
  token_web_data_->ShutdownOnUIThread();

#if defined(OS_WIN)
  password_web_data_->ShutdownOnUIThread();
#endif

  web_database_->ShutdownDatabase();
}

scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceWrapper::GetAutofillWebData() {
  return autofill_web_data_.get();
}

scoped_refptr<KeywordWebDataService>
WebDataServiceWrapper::GetKeywordWebData() {
  return keyword_web_data_.get();
}

scoped_refptr<TokenWebData> WebDataServiceWrapper::GetTokenWebData() {
  return token_web_data_.get();
}

#if defined(OS_WIN)
scoped_refptr<PasswordWebDataService>
WebDataServiceWrapper::GetPasswordWebData() {
  return password_web_data_.get();
}
#endif
