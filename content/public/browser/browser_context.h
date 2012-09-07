// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_

#include "base/callback_forward.h"
#include "base/hash_tables.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"

namespace appcache {
class AppCacheService;
}

namespace fileapi {
class FileSystemContext;
}

namespace net {
class URLRequestContextGetter;
}

namespace quota {
class SpecialStoragePolicy;
}

namespace webkit_database {
class DatabaseTracker;
}

class FilePath;
class GURL;

namespace content {

class DOMStorageContext;
class DownloadManager;
class DownloadManagerDelegate;
class GeolocationPermissionContext;
class IndexedDBContext;
class ResourceContext;
class SiteInstance;
class SpeechRecognitionPreferences;
class StoragePartition;

// This class holds the context needed for a browsing session.
// It lives on the UI thread. All these methods must only be called on the UI
// thread.
class CONTENT_EXPORT BrowserContext : public base::SupportsUserData {
 public:
  // Used in ForEachStoragePartition(). The first argument is the partition id.
  // The second argument is the StoragePartition object for that partition id.
  typedef base::Callback<void(const std::string&, StoragePartition*)>
      StoragePartitionCallback;

  static DownloadManager* GetDownloadManager(BrowserContext* browser_context);
  static IndexedDBContext* GetIndexedDBContext(BrowserContext* browser_context);
  static appcache::AppCacheService* GetAppCacheService(
      BrowserContext* browser_context);
  static fileapi::FileSystemContext* GetFileSystemContext(
      BrowserContext* browser_context);

  static content::StoragePartition* GetStoragePartition(
      BrowserContext* browser_context, SiteInstance* site_instance);
  static content::StoragePartition* GetStoragePartitionForSite(
      BrowserContext* browser_context, const GURL& site);
  static void ForEachStoragePartition(
      BrowserContext* browser_context,
      const StoragePartitionCallback& callback);

  // DON'T USE THIS. GetDefaultStoragePartition() is going away.
  // Use GetStoragePartition() instead. Ask ajwong@ if you have problems.
  static content::StoragePartition* GetDefaultStoragePartition(
      BrowserContext* browser_context);

  // Ensures that the corresponding ResourceContext is initialized. Normally the
  // BrowserContext initializs the corresponding getters when its objects are
  // created, but if the embedder wants to pass the ResourceContext to another
  // thread before they use BrowserContext, they should call this to make sure
  // that the ResourceContext is ready.
  static void EnsureResourceContextInitialized(BrowserContext* browser_context);

  // Tells the HTML5 objects on this context to persist their session state
  // across the next restart.
  static void SaveSessionState(BrowserContext* browser_context);

  // Tells the HTML5 objects on this context to purge any uneeded memory.
  static void PurgeMemory(BrowserContext* browser_context);

  virtual ~BrowserContext();

  // Returns the path of the directory where this context's data is stored.
  virtual FilePath GetPath() = 0;

  // Return whether this context is incognito. Default is false.
  // This doesn't belong here; http://crbug.com/89628
  virtual bool IsOffTheRecord() const = 0;

  // Returns the request context information associated with this context.  Call
  // this only on the UI thread, since it can send notifications that should
  // happen on the UI thread.
  // TODO(creis): Remove this version in favor of the one below.
  virtual net::URLRequestContextGetter* GetRequestContext() = 0;

  // Returns the request context appropriate for the given renderer. If the
  // renderer process doesn't have an associated installed app, or if the
  // installed app's is_storage_isolated() returns false, this is equivalent to
  // calling GetRequestContext().
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) = 0;

  // Returns the default request context for media resources associated with
  // this context.
  // TODO(creis): Remove this version in favor of the one below.
  virtual net::URLRequestContextGetter* GetMediaRequestContext() = 0;

  // Returns the request context for media resources associated with this
  // context and renderer process.
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) = 0;

  // Returns the resource context.
  virtual ResourceContext* GetResourceContext() = 0;

  // Returns the DownloadManagerDelegate for this context. This will be called
  // once per context. The embedder owns the delegate and is responsible for
  // ensuring that it outlives DownloadManager. It's valid to return NULL.
  virtual DownloadManagerDelegate* GetDownloadManagerDelegate() = 0;

  // Returns the geolocation permission context for this context. It's valid to
  // return NULL, in which case geolocation requests will always be allowed.
  virtual GeolocationPermissionContext* GetGeolocationPermissionContext() = 0;

  // Returns the speech input preferences. SpeechRecognitionPreferences is a
  // ref counted class, so callers should take a reference if needed. It's valid
  // to return NULL.
  virtual SpeechRecognitionPreferences* GetSpeechRecognitionPreferences() = 0;

  // Returns true if the last time this context was open it was exited cleanly.
  // This doesn't belong here; http://crbug.com/90737
  virtual bool DidLastSessionExitCleanly() = 0;

  // Returns a special storage policy implementation, or NULL.
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() = 0;
};

}  // namespace content

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {

template<>
struct hash<content::BrowserContext*> {
  std::size_t operator()(content::BrowserContext* const& p) const {
    return reinterpret_cast<std::size_t>(p);
  }
};

}  // namespace BASE_HASH_NAMESPACE
#endif

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_
