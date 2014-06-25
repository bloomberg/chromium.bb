// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/browser/webdata/logins_table.h"
#include "chrome/browser/webdata/web_apps_table.h"
#include "chrome/browser/webdata/web_intents_table.h"
#include "components/search_engines/template_url.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/webdata/common/web_database_service.h"
#include "content/public/browser/browser_thread.h"
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

WebDataService::KeywordBatchModeScoper::KeywordBatchModeScoper(
    WebDataService* service)
    : service_(service) {
  if (service_)
    service_->AdjustKeywordBatchModeLevel(true);
}

WebDataService::KeywordBatchModeScoper::~KeywordBatchModeScoper() {
  if (service_)
    service_->AdjustKeywordBatchModeLevel(false);
}

WebDataService::WebDataService(scoped_refptr<WebDatabaseService> wdbs,
                               const ProfileErrorCallback& callback)
    : WebDataServiceBase(
          wdbs, callback,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)),
      keyword_batch_mode_level_(0) {
}

//////////////////////////////////////////////////////////////////////////////
//
// Keywords.
//
//////////////////////////////////////////////////////////////////////////////

void WebDataService::AddKeyword(const TemplateURLData& data) {
  if (keyword_batch_mode_level_) {
    queued_keyword_operations_.push_back(
        KeywordTable::Operation(KeywordTable::ADD, data));
  } else {
    AdjustKeywordBatchModeLevel(true);
    AddKeyword(data);
    AdjustKeywordBatchModeLevel(false);
  }
}

void WebDataService::RemoveKeyword(TemplateURLID id) {
  if (keyword_batch_mode_level_) {
    TemplateURLData data;
    data.id = id;
    queued_keyword_operations_.push_back(
        KeywordTable::Operation(KeywordTable::REMOVE, data));
  } else {
    AdjustKeywordBatchModeLevel(true);
    RemoveKeyword(id);
    AdjustKeywordBatchModeLevel(false);
  }
}

void WebDataService::UpdateKeyword(const TemplateURLData& data) {
  if (keyword_batch_mode_level_) {
    queued_keyword_operations_.push_back(
        KeywordTable::Operation(KeywordTable::UPDATE, data));
  } else {
    AdjustKeywordBatchModeLevel(true);
    UpdateKeyword(data);
    AdjustKeywordBatchModeLevel(false);
  }
}

WebDataServiceBase::Handle WebDataService::GetKeywords(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE, Bind(&WebDataService::GetKeywordsImpl, this), consumer);
}

void WebDataService::SetDefaultSearchProviderID(TemplateURLID id) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      Bind(&WebDataService::SetDefaultSearchProviderIDImpl, this, id));
}

void WebDataService::SetBuiltinKeywordVersion(int version) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
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

WebDataService::WebDataService()
    : WebDataServiceBase(
          NULL, ProfileErrorCallback(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)),
      keyword_batch_mode_level_(0) {
}

WebDataService::~WebDataService() {
  DCHECK(!keyword_batch_mode_level_);
}

void WebDataService::AdjustKeywordBatchModeLevel(bool entering_batch_mode) {
  if (entering_batch_mode) {
    ++keyword_batch_mode_level_;
  } else {
    DCHECK(keyword_batch_mode_level_);
    --keyword_batch_mode_level_;
    if (!keyword_batch_mode_level_ && !queued_keyword_operations_.empty()) {
      wdbs_->ScheduleDBTask(
          FROM_HERE,
          Bind(&WebDataService::PerformKeywordOperationsImpl, this,
               queued_keyword_operations_));
      queued_keyword_operations_.clear();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// Keywords implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDatabase::State WebDataService::PerformKeywordOperationsImpl(
    const KeywordTable::Operations& operations,
    WebDatabase* db) {
  return KeywordTable::FromWebDatabase(db)->PerformOperations(operations) ?
      WebDatabase::COMMIT_NEEDED : WebDatabase::COMMIT_NOT_NEEDED;
}

scoped_ptr<WDTypedResult> WebDataService::GetKeywordsImpl(WebDatabase* db) {
  scoped_ptr<WDTypedResult> result_ptr;
  WDKeywordsResult result;
  if (KeywordTable::FromWebDatabase(db)->GetKeywords(&result.keywords)) {
    result.default_search_provider_id =
        KeywordTable::FromWebDatabase(db)->GetDefaultSearchProviderID();
    result.builtin_keyword_version =
        KeywordTable::FromWebDatabase(db)->GetBuiltinKeywordVersion();
    result_ptr.reset(new WDResult<WDKeywordsResult>(KEYWORDS_RESULT, result));
  }
  return result_ptr.Pass();
}

WebDatabase::State WebDataService::SetDefaultSearchProviderIDImpl(
    TemplateURLID id,
    WebDatabase* db) {
  return KeywordTable::FromWebDatabase(db)->SetDefaultSearchProviderID(id) ?
      WebDatabase::COMMIT_NEEDED : WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State WebDataService::SetBuiltinKeywordVersionImpl(
    int version,
    WebDatabase* db) {
  return KeywordTable::FromWebDatabase(db)->SetBuiltinKeywordVersion(version) ?
      WebDatabase::COMMIT_NEEDED : WebDatabase::COMMIT_NOT_NEEDED;
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
