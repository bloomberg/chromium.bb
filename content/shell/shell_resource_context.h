// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_RESOURCE_CONTEXT_H_
#define CONTENT_SHELL_SHELL_RESOURCE_CONTEXT_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/resource_context.h"

class ChromeBlobStorageContext;

namespace content {

class ShellURLRequestContextGetter;

class ShellResourceContext : public content::ResourceContext {
 public:
  ShellResourceContext(
      ShellURLRequestContextGetter* getter,
      ChromeBlobStorageContext* blob_storage_context);
  virtual ~ShellResourceContext();

 private:
  // ResourceContext implementation:
  virtual net::HostResolver* GetHostResolver() OVERRIDE;
  virtual net::URLRequestContext* GetRequestContext() OVERRIDE;
  virtual ChromeAppCacheService* GetAppCacheService() OVERRIDE;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() OVERRIDE;
  virtual fileapi::FileSystemContext* GetFileSystemContext() OVERRIDE;
  virtual ChromeBlobStorageContext* GetBlobStorageContext() OVERRIDE;
  virtual quota::QuotaManager* GetQuotaManager() OVERRIDE;
  virtual HostZoomMap* GetHostZoomMap() OVERRIDE;
  virtual MediaObserver* GetMediaObserver() OVERRIDE;
  virtual media_stream::MediaStreamManager* GetMediaStreamManager() OVERRIDE;
  virtual AudioManager* GetAudioManager() OVERRIDE;
  virtual WebKitContext* GetWebKitContext() OVERRIDE;

  scoped_refptr<ShellURLRequestContextGetter> getter_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  DISALLOW_COPY_AND_ASSIGN(ShellResourceContext);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_RESOURCE_CONTEXT_H_
