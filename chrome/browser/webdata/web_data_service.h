// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__
#define CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"

struct DefaultWebIntentService;
class GURL;
#if defined(OS_WIN)
struct IE7PasswordInfo;
#endif
class Profile;
class SkBitmap;
class WebDatabaseService;

namespace base {
class Thread;
}

namespace content {
class BrowserContext;
}

namespace webkit_glue {
struct WebIntentServiceData;
}

////////////////////////////////////////////////////////////////////////////////
//
// WebDataService is a generic data repository for meta data associated with
// web pages. All data is retrieved and archived in an asynchronous way.
//
// All requests return a handle. The handle can be used to cancel the request.
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
// WebDataService results
//
////////////////////////////////////////////////////////////////////////////////

// Result from GetWebAppImages.
struct WDAppImagesResult {
  WDAppImagesResult();
  ~WDAppImagesResult();

  // True if SetWebAppHasAllImages(true) was invoked.
  bool has_all_images;

  // The images, may be empty.
  std::vector<SkBitmap> images;
};

class WebDataServiceConsumer;

class WebDataService : public WebDataServiceBase {
 public:
  // Retrieve a WebDataService for the given context.
  static scoped_refptr<WebDataService> FromBrowserContext(
      content::BrowserContext* context);

  WebDataService(scoped_refptr<WebDatabaseService> wdbs,
                 const ProfileErrorCallback& callback);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Apps
  //
  //////////////////////////////////////////////////////////////////////////////

  // Sets the image for the specified web app. A web app can have any number of
  // images, but only one at a particular size. If there was an image for the
  // web app at the size of the given image it is replaced.
  void SetWebAppImage(const GURL& app_url, const SkBitmap& image);

  // Sets whether all the images have been downloaded for the specified web app.
  void SetWebAppHasAllImages(const GURL& app_url, bool has_all_images);

  // Removes all images for the specified web app.
  void RemoveWebApp(const GURL& app_url);

  // Fetches the images and whether all images have been downloaded for the
  // specified web app.
  Handle GetWebAppImages(const GURL& app_url, WebDataServiceConsumer* consumer);

#if defined(OS_WIN)
  //////////////////////////////////////////////////////////////////////////////
  //
  // IE7/8 Password Access (used by PasswordStoreWin - do not use elsewhere)
  //
  //////////////////////////////////////////////////////////////////////////////

  // Adds |info| to the list of imported passwords from ie7/ie8.
  void AddIE7Login(const IE7PasswordInfo& info);

  // Removes |info| from the list of imported passwords from ie7/ie8.
  void RemoveIE7Login(const IE7PasswordInfo& info);

  // Get the login matching the information in |info|. |consumer| will be
  // notified when the request is done. The result is of type
  // WDResult<IE7PasswordInfo>.
  // If there is no match, the fields of the IE7PasswordInfo will be empty.
  Handle GetIE7Login(const IE7PasswordInfo& info,
                     WebDataServiceConsumer* consumer);
#endif  // defined(OS_WIN)

 protected:
  // For unit tests, passes a null callback.
  WebDataService();

  virtual ~WebDataService();

 private:
  //////////////////////////////////////////////////////////////////////////////
  //
  // The following methods are only invoked on the DB thread.
  //
  //////////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Apps.
  //
  //////////////////////////////////////////////////////////////////////////////

  WebDatabase::State SetWebAppImageImpl(const GURL& app_url,
      const SkBitmap& image, WebDatabase* db);
  WebDatabase::State SetWebAppHasAllImagesImpl(const GURL& app_url,
      bool has_all_images, WebDatabase* db);
  WebDatabase::State RemoveWebAppImpl(const GURL& app_url, WebDatabase* db);
  scoped_ptr<WDTypedResult> GetWebAppImagesImpl(
      const GURL& app_url, WebDatabase* db);

#if defined(ENABLE_WEB_INTENTS)
  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Intents.
  //
  //////////////////////////////////////////////////////////////////////////////
  WebDatabase::State AddWebIntentServiceImpl(
      const webkit_glue::WebIntentServiceData& service);
  WebDatabase::State RemoveWebIntentServiceImpl(
      const webkit_glue::WebIntentServiceData& service);
  scoped_ptr<WDTypedResult> GetWebIntentServicesImpl(
      const base::string16& action);
  scoped_ptr<WDTypedResult> GetWebIntentServicesForURLImpl(
      const base::string16& service_url);
  scoped_ptr<WDTypedResult> GetAllWebIntentServicesImpl();
  WebDatabase::State AddDefaultWebIntentServiceImpl(
      const DefaultWebIntentService& service);
  WebDatabase::State RemoveDefaultWebIntentServiceImpl(
      const DefaultWebIntentService& service);
  WebDatabase::State RemoveWebIntentServiceDefaultsImpl(
      const GURL& service_url);
  scoped_ptr<WDTypedResult> GetDefaultWebIntentServicesForActionImpl(
      const base::string16& action);
  scoped_ptr<WDTypedResult> GetAllDefaultWebIntentServicesImpl();
#endif

#if defined(OS_WIN)
  //////////////////////////////////////////////////////////////////////////////
  //
  // Password manager.
  //
  //////////////////////////////////////////////////////////////////////////////
  WebDatabase::State AddIE7LoginImpl(
      const IE7PasswordInfo& info, WebDatabase* db);
  WebDatabase::State RemoveIE7LoginImpl(
      const IE7PasswordInfo& info, WebDatabase* db);
  scoped_ptr<WDTypedResult> GetIE7LoginImpl(
      const IE7PasswordInfo& info, WebDatabase* db);
#endif  // defined(OS_WIN)

  DISALLOW_COPY_AND_ASSIGN(WebDataService);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__
