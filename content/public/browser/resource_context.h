// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_

#include "base/basictypes.h"

class AudioManager;
class ChromeAppCacheService;
class ChromeBlobStorageContext;
class MediaObserver;
class WebKitContext;
namespace fileapi {
class FileSystemContext;
}  // namespace fileapi
namespace media_stream {
class MediaStreamManager;
}  // namespace media_stream
namespace net {
class HostResolver;
class URLRequestContext;
}  // namespace net
namespace quota {
class QuotaManager;
};  // namespace quota
namespace webkit_database {
class DatabaseTracker;
}  // namespace webkit_database

namespace content {

class HostZoomMap;

// ResourceContext contains the relevant context information required for
// resource loading. It lives on the IO thread, although it is constructed on
// the UI thread.
class ResourceContext {
 public:
  virtual ~ResourceContext() {}

  virtual net::HostResolver* GetHostResolver() = 0;
  virtual net::URLRequestContext* GetRequestContext() = 0;
  virtual ChromeAppCacheService* GetAppCacheService() = 0;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() = 0;
  virtual fileapi::FileSystemContext* GetFileSystemContext() = 0;
  virtual ChromeBlobStorageContext* GetBlobStorageContext() = 0;
  virtual quota::QuotaManager* GetQuotaManager() = 0;
  virtual HostZoomMap* GetHostZoomMap() = 0;
  virtual MediaObserver* GetMediaObserver() = 0;
  virtual media_stream::MediaStreamManager* GetMediaStreamManager() = 0;
  virtual AudioManager* GetAudioManager() = 0;
  virtual WebKitContext* GetWebKitContext() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
