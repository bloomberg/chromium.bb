// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_RESOURCE_GETTER_IMPL_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_RESOURCE_GETTER_IMPL_H_

#include <jni.h>
#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/android/media_resource_getter.h"
#include "media/base/android/media_url_interceptor.h"
#include "net/base/auth.h"
#include "net/cookies/canonical_cookie.h"

namespace storage {
class FileSystemContext;
}

namespace content {

class BrowserContext;
class ResourceContext;

// This class implements media::MediaResourceGetter to retrieve resources
// asynchronously on the UI thread.
class MediaResourceGetterImpl : public media::MediaResourceGetter {
 public:
  // Construct a MediaResourceGetterImpl object. |browser_context| and
  // |render_process_id| are passed to retrieve the CookieStore.
  // |file_system_context| are used to get the platform path.
  MediaResourceGetterImpl(BrowserContext* browser_context,
                          storage::FileSystemContext* file_system_context,
                          int render_process_id,
                          int render_frame_id);
  ~MediaResourceGetterImpl() override;

  // media::MediaResourceGetter implementation.
  // Must be called on the UI thread.
  void GetAuthCredentials(const GURL& url,
                          GetAuthCredentialsCB callback) override;
  void GetCookies(const GURL& url,
                  const GURL& site_for_cookies,
                  GetCookieCB callback) override;
  void GetPlatformPathFromURL(const GURL& url,
                              GetPlatformPathCB callback) override;
  void ExtractMediaMetadata(const std::string& url,
                            const std::string& cookies,
                            const std::string& user_agent,
                            ExtractMediaMetadataCB callback) override;
  void ExtractMediaMetadata(const int fd,
                            const int64_t offset,
                            const int64_t size,
                            ExtractMediaMetadataCB callback) override;

 private:
  // Called when GetAuthCredentials() finishes.
  void GetAuthCredentialsCallback(GetAuthCredentialsCB callback,
                                  const net::AuthCredentials& credentials);

  // Called when GetCookies() finishes.
  void GetCookiesCallback(GetCookieCB callback, const std::string& cookies);

  // Called when GetPlatformPathFromFileSystemURL() finishes.
  void GetPlatformPathCallback(GetPlatformPathCB callback,
                               const std::string& platform_path);

  // BrowserContext to retrieve URLRequestContext and ResourceContext.
  BrowserContext* browser_context_;

  // FileSystemContext to be used on FILE thread.
  storage::FileSystemContext* file_system_context_;

  // Render process id, used to check whether the process can access cookies.
  int render_process_id_;

  // Render frame id, used to check tab specific cookie policy.
  int render_frame_id_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaResourceGetterImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaResourceGetterImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_RESOURCE_GETTER_IMPL_H_
