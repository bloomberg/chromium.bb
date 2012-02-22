// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_

#include "base/basictypes.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"

class AudioManager;
class MediaObserver;

namespace appcache {
class AppCacheService;
}

namespace fileapi {
class FileSystemContext;
}

namespace media_stream {
class MediaStreamManager;
}

namespace net {
class HostResolver;
class URLRequestContext;
}

namespace webkit_blob {
class BlobStorageController;
}

namespace content {

// ResourceContext contains the relevant context information required for
// resource loading. It lives on the IO thread, although it is constructed on
// the UI thread.
class CONTENT_EXPORT ResourceContext : public base::SupportsUserData {
 public:
  static appcache::AppCacheService* GetAppCacheService(
      ResourceContext* resource_context);
  static fileapi::FileSystemContext* GetFileSystemContext(
      ResourceContext* resource_context);
  static webkit_blob::BlobStorageController* GetBlobStorageController(
      ResourceContext* resource_context);

  virtual ~ResourceContext() {}
  virtual net::HostResolver* GetHostResolver() = 0;
  virtual net::URLRequestContext* GetRequestContext() = 0;
  virtual MediaObserver* GetMediaObserver() = 0;
  virtual media_stream::MediaStreamManager* GetMediaStreamManager() = 0;
  virtual AudioManager* GetAudioManager() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
