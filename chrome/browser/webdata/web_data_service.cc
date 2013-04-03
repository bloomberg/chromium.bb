// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service.h"

#include "base/stl_util.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/browser/webdata/logins_table.h"
#include "chrome/browser/webdata/token_service_table.h"
#include "chrome/browser/webdata/web_apps_table.h"
#include "chrome/browser/webdata/web_intents_table.h"
#include "chrome/common/chrome_notification_types.h"
#include "components/webdata/common/web_database_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "third_party/skia/include/core/SkBitmap.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataService implementation.
//
////////////////////////////////////////////////////////////////////////////////

using base::Bind;
using base::Time;
using content::BrowserThread;

WDAppImagesResult::WDAppImagesResult() : has_all_images(false) {}

WDAppImagesResult::~WDAppImagesResult() {}

WDKeywordsResult::WDKeywordsResult()
  : default_search_provider_id(0),
    builtin_keyword_version(0) {
}

WDKeywordsResult::~WDKeywordsResult() {}

WebDataService::WebDataService(scoped_refptr<WebDatabaseService> wdbs,
                               const ProfileErrorCallback& callback)
    : WebDataServiceBase(wdbs, callback) {
}

//////////////////////////////////////////////////////////////////////////////
//
// Keywords.
//
//////////////////////////////////////////////////////////////////////////////

void WebDataService::AddKeyword(const TemplateURLData& data) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&WebDataService::AddKeywordImpl, this, data));
}

void WebDataService::RemoveKeyword(TemplateURLID id) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&WebDataService::RemoveKeywordImpl, this, id));
}

void WebDataService::UpdateKeyword(const TemplateURLData& data) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&WebDataService::UpdateKeywordImpl, this, data));
}

WebDataServiceBase::Handle WebDataService::GetKeywords(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetKeywordsImpl, this), consumer);
}

void WebDataService::SetDefaultSearchProvider(const TemplateURL* url) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetDefaultSearchProviderImpl, this,
           url ? url->id() : 0));
}

void WebDataService::SetBuiltinKeywordVersion(int version) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetBuiltinKeywordVersionImpl, this, version));
}

//////////////////////////////////////////////////////////////////////////////
//
// Web Apps
//
//////////////////////////////////////////////////////////////////////////////

void WebDataService::SetWebAppImage(const GURL& app_url,
                                    const SkBitmap& image) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetWebAppImageImpl, this, app_url, image));
}

void WebDataService::SetWebAppHasAllImages(const GURL& app_url,
                                           bool has_all_images) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetWebAppHasAllImagesImpl, this, app_url,
           has_all_images));
}

void WebDataService::RemoveWebApp(const GURL& app_url) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveWebAppImpl, this, app_url));
}

WebDataServiceBase::Handle WebDataService::GetWebAppImages(
    const GURL& app_url, WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetWebAppImagesImpl, this, app_url), consumer);
}

////////////////////////////////////////////////////////////////////////////////
//
// Token Service
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::SetTokenForService(const std::string& service,
                                        const std::string& token) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetTokenForServiceImpl, this, service, token));
}

void WebDataService::RemoveAllTokens() {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveAllTokensImpl, this));
}

// Null on failure. Success is WDResult<std::string>
WebDataServiceBase::Handle WebDataService::GetAllTokens(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetAllTokensImpl, this), consumer);
}

////////////////////////////////////////////////////////////////////////////////

WebDataService::WebDataService()
    : WebDataServiceBase(NULL, ProfileErrorCallback()) {
}

WebDataService::~WebDataService() {
}

////////////////////////////////////////////////////////////////////////////////
//
// Keywords implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDatabase::State WebDataService::AddKeywordImpl(
    const TemplateURLData& data, WebDatabase* db) {
  KeywordTable::FromWebDatabase(db)->AddKeyword(data);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::RemoveKeywordImpl(
    TemplateURLID id, WebDatabase* db) {
  DCHECK(id);
  KeywordTable::FromWebDatabase(db)->RemoveKeyword(id);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::UpdateKeywordImpl(
    const TemplateURLData& data, WebDatabase* db) {
  if (!KeywordTable::FromWebDatabase(db)->UpdateKeyword(data)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
 return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> WebDataService::GetKeywordsImpl(WebDatabase* db) {
  WDKeywordsResult result;
  KeywordTable::FromWebDatabase(db)->GetKeywords(&result.keywords);
  result.default_search_provider_id =
      KeywordTable::FromWebDatabase(db)->GetDefaultSearchProviderID();
  result.builtin_keyword_version =
      KeywordTable::FromWebDatabase(db)->GetBuiltinKeywordVersion();
  return scoped_ptr<WDTypedResult>(
      new WDResult<WDKeywordsResult>(KEYWORDS_RESULT, result));
}

WebDatabase::State WebDataService::SetDefaultSearchProviderImpl(
    TemplateURLID id, WebDatabase* db) {
  if (!KeywordTable::FromWebDatabase(db)->SetDefaultSearchProviderID(id)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::SetBuiltinKeywordVersionImpl(
    int version, WebDatabase* db) {
  if (!KeywordTable::FromWebDatabase(db)->SetBuiltinKeywordVersion(version)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  return WebDatabase::COMMIT_NEEDED;
}

////////////////////////////////////////////////////////////////////////////////
//
// Web Apps implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDatabase::State WebDataService::SetWebAppImageImpl(
    const GURL& app_url, const SkBitmap& image, WebDatabase* db) {
  WebAppsTable::FromWebDatabase(db)->SetWebAppImage(app_url, image);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::SetWebAppHasAllImagesImpl(
    const GURL& app_url, bool has_all_images, WebDatabase* db) {
  WebAppsTable::FromWebDatabase(db)->SetWebAppHasAllImages(app_url,
                                                           has_all_images);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::RemoveWebAppImpl(
    const GURL& app_url, WebDatabase* db) {
  WebAppsTable::FromWebDatabase(db)->RemoveWebApp(app_url);
  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> WebDataService::GetWebAppImagesImpl(
    const GURL& app_url, WebDatabase* db) {
  WDAppImagesResult result;
  result.has_all_images =
      WebAppsTable::FromWebDatabase(db)->GetWebAppHasAllImages(app_url);
  WebAppsTable::FromWebDatabase(db)->GetWebAppImages(app_url, &result.images);
  return scoped_ptr<WDTypedResult>(
      new WDResult<WDAppImagesResult>(WEB_APP_IMAGES, result));
}

////////////////////////////////////////////////////////////////////////////////
//
// Token Service implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDatabase::State WebDataService::RemoveAllTokensImpl(WebDatabase* db) {
  if (TokenServiceTable::FromWebDatabase(db)->RemoveAllTokens()) {
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State WebDataService::SetTokenForServiceImpl(
    const std::string& service, const std::string& token, WebDatabase* db) {
  if (TokenServiceTable::FromWebDatabase(db)->SetTokenForService(service,
                                                                 token)) {
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

scoped_ptr<WDTypedResult> WebDataService::GetAllTokensImpl(WebDatabase* db) {
  std::map<std::string, std::string> map;
  TokenServiceTable::FromWebDatabase(db)->GetAllTokens(&map);
  return scoped_ptr<WDTypedResult>(
      new WDResult<std::map<std::string, std::string> >(TOKEN_RESULT, map));
}
