// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RESOURCE_CONTEXT_H_
#define CONTENT_BROWSER_RESOURCE_CONTEXT_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

class ChromeAppCacheService;
class ChromeBlobStorageContext;
class DownloadIdFactory;
class HostZoomMap;
class MediaObserver;
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

// ResourceContext contains the relevant context information required for
// resource loading. It lives on the IO thread, although it is constructed on
// the UI thread. ResourceContext doesn't own anything it points to, it just
// holds pointers to relevant objects to resource loading.
class CONTENT_EXPORT ResourceContext {
 public:
  virtual ~ResourceContext();

  // The user data allows the clients to associate data with this request.
  // Multiple user data values can be stored under different keys.
  void* GetUserData(const void* key) const;
  void SetUserData(const void* key, void* data);

  net::HostResolver* host_resolver() const;
  void set_host_resolver(net::HostResolver* host_resolver);

  net::URLRequestContext* request_context() const;
  void set_request_context(net::URLRequestContext* request_context);

  ChromeAppCacheService* appcache_service() const;
  void set_appcache_service(ChromeAppCacheService* service);

  webkit_database::DatabaseTracker* database_tracker() const;
  void set_database_tracker(webkit_database::DatabaseTracker* tracker);

  fileapi::FileSystemContext* file_system_context() const;
  void set_file_system_context(fileapi::FileSystemContext* context);

  ChromeBlobStorageContext* blob_storage_context() const;
  void set_blob_storage_context(ChromeBlobStorageContext* context);

  quota::QuotaManager* quota_manager() const;
  void set_quota_manager(quota::QuotaManager* quota_manager);

  HostZoomMap* host_zoom_map() const;
  void set_host_zoom_map(HostZoomMap* host_zoom_map);

  MediaObserver* media_observer() const;
  void set_media_observer(MediaObserver* media_observer);

  DownloadIdFactory* download_id_factory() const;
  void set_download_id_factory(DownloadIdFactory* download_id_factory);

  media_stream::MediaStreamManager* media_stream_manager() const;
  void set_media_stream_manager(
      media_stream::MediaStreamManager* media_stream_manager);

 protected:
  ResourceContext();

 private:
  virtual void EnsureInitialized() const = 0;

  net::HostResolver* host_resolver_;
  net::URLRequestContext* request_context_;
  ChromeAppCacheService* appcache_service_;
  webkit_database::DatabaseTracker* database_tracker_;
  fileapi::FileSystemContext* file_system_context_;
  ChromeBlobStorageContext* blob_storage_context_;
  quota::QuotaManager* quota_manager_;
  HostZoomMap* host_zoom_map_;
  MediaObserver* media_observer_;
  DownloadIdFactory* download_id_factory_;
  media_stream::MediaStreamManager* media_stream_manager_;

  // Externally-defined data accessible by key.
  typedef std::map<const void*, void*> UserDataMap;
  UserDataMap user_data_;

  DISALLOW_COPY_AND_ASSIGN(ResourceContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RESOURCE_CONTEXT_H_
