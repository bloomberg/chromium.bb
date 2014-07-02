// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service.h"

#include "base/bind.h"
#include "chrome/browser/webdata/logins_table.h"
#include "chrome/browser/webdata/web_apps_table.h"
#include "chrome/browser/webdata/web_intents_table.h"
#include "components/search_engines/template_url.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/webdata/common/web_database_service.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataService implementation.
//
////////////////////////////////////////////////////////////////////////////////

using base::Bind;
using content::BrowserThread;

WDAppImagesResult::WDAppImagesResult() : has_all_images(false) {}

WDAppImagesResult::~WDAppImagesResult() {}

WebDataService::WebDataService(scoped_refptr<WebDatabaseService> wdbs,
                               const ProfileErrorCallback& callback)
    : WebDataServiceBase(
          wdbs, callback,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)) {
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
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)) {
}

WebDataService::~WebDataService() {
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
