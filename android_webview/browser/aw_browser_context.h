// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_

#include "android_webview/browser/aw_download_manager_delegate.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/geolocation_permission_context.h"

namespace android_webview {

class AwURLRequestContextGetter;

typedef content::GeolocationPermissionContext* GeolocationPermissionFactoryFn();

class AwBrowserContext : public content::BrowserContext {
 public:
  AwBrowserContext(
      const FilePath path,
      GeolocationPermissionFactoryFn* geolocation_permission_factory);
  virtual ~AwBrowserContext();

  // Called before BrowserThreads are created.
  void InitializeBeforeThreadCreation();

  // content::BrowserContext implementation.
  virtual FilePath GetPath() OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForStoragePartition(
      const FilePath& partition_path, bool in_memory) OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const FilePath& partition_path, bool in_memory) OVERRIDE;
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() OVERRIDE;
  virtual content::GeolocationPermissionContext*
      GetGeolocationPermissionContext() OVERRIDE;
  virtual content::SpeechRecognitionPreferences*
      GetSpeechRecognitionPreferences() OVERRIDE;
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;

 private:

  // The file path where data for this context is persisted.
  FilePath context_storage_path_;

  scoped_refptr<AwURLRequestContextGetter> url_request_context_getter_;
  GeolocationPermissionFactoryFn* geolocation_permission_factory_;
  scoped_refptr<content::GeolocationPermissionContext>
      geolocation_permission_context_;

  AwDownloadManagerDelegate download_manager_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AwBrowserContext);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
