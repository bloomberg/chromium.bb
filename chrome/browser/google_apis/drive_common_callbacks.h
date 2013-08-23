// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains callback types used for communicating with the Drive
// server via WAPI (Documents List API) and Drive API.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_COMMON_CALLBACKS_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_COMMON_CALLBACKS_H_

#include "chrome/browser/google_apis/base_requests.h"

namespace google_apis {

class AboutResource;
class AppList;
class ResourceEntry;
class ResourceList;

// Callback used for getting ResourceList.
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<ResourceList> resource_list)>
    GetResourceListCallback;

// Callback used for getting ResourceEntry.
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<ResourceEntry> entry)>
    GetResourceEntryCallback;

// Callback used for getting AboutResource.
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<AboutResource> about_resource)>
    AboutResourceCallback;

// Callback used for getting ShareUrl.
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& share_url)> GetShareUrlCallback;

// Callback used for getting AppList.
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<AppList> app_list)> AppListCallback;

// Callback used for handling UploadRangeResponse.
typedef base::Callback<void(
    const UploadRangeResponse& response,
    scoped_ptr<ResourceEntry> new_entry)> UploadRangeCallback;

// Callback used for authorizing an app. |open_url| is used to open the target
// file with the authorized app.
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& open_url)>
    AuthorizeAppCallback;

// Closure for canceling a certain request. Each request-issuing method returns
// this type of closure. If it is called during the request is in-flight, the
// callback passed with the request is invoked with GDATA_CANCELLED. If the
// request is already finished, nothing happens.
typedef base::Closure CancelCallback;

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_COMMON_CALLBACKS_H_
