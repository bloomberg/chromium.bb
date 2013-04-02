// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_MEDIA_RESOURCE_GETTER_IMPL_H_
#define CONTENT_BROWSER_ANDROID_MEDIA_RESOURCE_GETTER_IMPL_H_

#include <jni.h>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/android/media_resource_getter.h"
#include "net/cookies/canonical_cookie.h"

namespace fileapi {
class FileSystemContext;
}

namespace net {
class URLRequestContextGetter;
}

namespace content {

class BrowserContext;
class ResourceContext;

// This class implements media::MediaResourceGetter to retrieve resources
// asynchronously on the UI thread.
class MediaResourceGetterImpl : public media::MediaResourceGetter {
 public:
  // Construct a MediaResourceGetterImpl object. |browser_context| and
  // |renderer_id| are passed to retrieve the CookieStore.
  // |file_system_context| are used to get the platform path.
  MediaResourceGetterImpl(BrowserContext* browser_context,
                          fileapi::FileSystemContext* file_system_context,
                          int renderer_id, int routing_id);
  virtual ~MediaResourceGetterImpl();

  // media::MediaResourceGetter implementation.
  // Must be called on the UI thread.
  virtual void GetCookies(const GURL& url,
                          const GURL& first_party_for_cookies,
                          const GetCookieCB& callback) OVERRIDE;
  virtual void GetPlatformPathFromFileSystemURL(
      const GURL& url,
      const GetPlatformPathCB& callback) OVERRIDE;
  virtual void ExtractMediaMetadata(
      const std::string& url, const std::string& cookies,
      const ExtractMediaMetadataCB& callback) OVERRIDE;

  static bool RegisterMediaResourceGetter(JNIEnv* env);

 private:
  // Called when GetCookies() finishes.
  void GetCookiesCallback(
      const GetCookieCB& callback, const std::string& cookies);

  // Called when GetPlatformPathFromFileSystemURL() finishes.
  void GetPlatformPathCallback(
      const GetPlatformPathCB& callback, const std::string& platform_path);

  // BrowserContext to retrieve URLRequestContext and ResourceContext.
  BrowserContext* browser_context_;

  // FileSystemContext to be used on FILE thread.
  fileapi::FileSystemContext* file_system_context_;

  // Used to post tasks.
  base::WeakPtrFactory<MediaResourceGetterImpl> weak_this_;

  // Render process id, used to check whether the process can access cookies.
  int renderer_id_;

  // Routing id for the render view, used to check tab specific cookie policy.
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(MediaResourceGetterImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_MEDIA_RESOURCE_GETTER_IMPL_H_
