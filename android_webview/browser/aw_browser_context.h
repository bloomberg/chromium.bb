// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_

#include <vector>

#include "android_webview/browser/aw_download_manager_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/geolocation_permission_context.h"
#include "net/url_request/url_request_job_factory.h"

class GURL;

namespace components {
class VisitedLinkMaster;
}  // namespace components

namespace content {
class ResourceContext;
class WebContents;
}  // namespace content

namespace android_webview {

class AwURLRequestContextGetter;
class AwQuotaManagerBridge;
class JniDependencyFactory;

class AwBrowserContext : public content::BrowserContext,
                         public components::VisitedLinkDelegate {
 public:

  AwBrowserContext(const base::FilePath path,
                   JniDependencyFactory* native_factory);
  virtual ~AwBrowserContext();

  // Convenience method to returns the AwBrowserContext corresponding to the
  // given WebContents.
  static AwBrowserContext* FromWebContents(
      content::WebContents* web_contents);

  // Called before BrowserThreads are created.
  void InitializeBeforeThreadCreation();

  // Maps to BrowserMainParts::PreMainMessageLoopRun.
  void PreMainMessageLoopRun();

  // These methods map to Add methods in components::VisitedLinkMaster.
  void AddVisitedURLs(const std::vector<GURL>& urls);

  net::URLRequestContextGetter* CreateRequestContext(
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          blob_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          file_system_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          developer_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_devtools_protocol_handler);
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          blob_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          file_system_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          developer_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_devtools_protocol_handler);

  AwQuotaManagerBridge* GetQuotaManagerBridge();

  // content::BrowserContext implementation.
  virtual base::FilePath GetPath() OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path, bool in_memory) OVERRIDE;
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() OVERRIDE;
  virtual content::GeolocationPermissionContext*
      GetGeolocationPermissionContext() OVERRIDE;
  virtual content::SpeechRecognitionPreferences*
      GetSpeechRecognitionPreferences() OVERRIDE;
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;

  // components::VisitedLinkDelegate implementation.
  virtual void RebuildTable(
      const scoped_refptr<URLEnumerator>& enumerator) OVERRIDE;

 private:
  // The file path where data for this context is persisted.
  base::FilePath context_storage_path_;

  JniDependencyFactory* native_factory_;
  scoped_refptr<AwURLRequestContextGetter> url_request_context_getter_;
  scoped_refptr<content::GeolocationPermissionContext>
      geolocation_permission_context_;
  scoped_ptr<AwQuotaManagerBridge> quota_manager_bridge_;

  AwDownloadManagerDelegate download_manager_delegate_;

  scoped_ptr<components::VisitedLinkMaster> visitedlink_master_;
  scoped_ptr<content::ResourceContext> resource_context_;

  DISALLOW_COPY_AND_ASSIGN(AwBrowserContext);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
