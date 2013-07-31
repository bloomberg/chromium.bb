// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/token_web_data.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/webdata/token_service_table.h"
#include "components/webdata/common/web_database_service.h"
#include "content/public/browser/browser_thread.h"

using base::Bind;
using base::Time;
using content::BrowserThread;

class TokenWebDataBackend
    : public base::RefCountedThreadSafe<TokenWebDataBackend,
                                        BrowserThread::DeleteOnDBThread> {

 public:
  TokenWebDataBackend() {
  }

  WebDatabase::State RemoveAllTokens(WebDatabase* db) {
    if (TokenServiceTable::FromWebDatabase(db)->RemoveAllTokens()) {
      return WebDatabase::COMMIT_NEEDED;
    }
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  WebDatabase::State RemoveTokenForService(
      const std::string& service, WebDatabase* db) {
    if (TokenServiceTable::FromWebDatabase(db)
            ->RemoveTokenForService(service)) {
      return WebDatabase::COMMIT_NEEDED;
    }
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  WebDatabase::State SetTokenForService(
      const std::string& service, const std::string& token, WebDatabase* db) {
    if (TokenServiceTable::FromWebDatabase(db)->SetTokenForService(service,
                                                                   token)) {
      return WebDatabase::COMMIT_NEEDED;
    }
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  scoped_ptr<WDTypedResult> GetAllTokens(WebDatabase* db) {
    std::map<std::string, std::string> map;
    TokenServiceTable::FromWebDatabase(db)->GetAllTokens(&map);
    return scoped_ptr<WDTypedResult>(
        new WDResult<std::map<std::string, std::string> >(TOKEN_RESULT, map));
  }

 protected:
  virtual ~TokenWebDataBackend() {
  }

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::DB>;
  friend class base::DeleteHelper<TokenWebDataBackend>;
  // We have to friend RCTS<> so WIN shared-lib build is happy
  // (http://crbug/112250).
  friend class base::RefCountedThreadSafe<TokenWebDataBackend,
                                          BrowserThread::DeleteOnDBThread>;

};

TokenWebData::TokenWebData(scoped_refptr<WebDatabaseService> wdbs,
                               const ProfileErrorCallback& callback)
    : WebDataServiceBase(wdbs, callback,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)),
      token_backend_(new TokenWebDataBackend()) {
}

void TokenWebData::SetTokenForService(const std::string& service,
                                      const std::string& token) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&TokenWebDataBackend::SetTokenForService, token_backend_,
           service, token));
}

void TokenWebData::RemoveAllTokens() {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&TokenWebDataBackend::RemoveAllTokens, token_backend_));
}

void TokenWebData::RemoveTokenForService(const std::string& service) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&TokenWebDataBackend::RemoveTokenForService, token_backend_,
           service));
}

// Null on failure. Success is WDResult<std::string>
WebDataServiceBase::Handle TokenWebData::GetAllTokens(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&TokenWebDataBackend::GetAllTokens, token_backend_), consumer);
}

TokenWebData::TokenWebData()
    : WebDataServiceBase(NULL, ProfileErrorCallback(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)),
      token_backend_(new TokenWebDataBackend()) {
}

TokenWebData::~TokenWebData() {
}
