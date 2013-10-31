// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_BROWSER_CONTEXT_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_BROWSER_CONTEXT_H_

#include "content/public/browser/browser_context.h"

#include "base/files/file_path.h"

namespace content {

class DownloadManagerDelegate;
class GeolocationPermissionContext;
class ResourceContext;

}

class FakeBrowserContext : public content::BrowserContext {
 public:
  FakeBrowserContext(const std::string& name, const base::FilePath& path);
  virtual ~FakeBrowserContext();

  const std::string& name() const { return name_; }

  virtual base::FilePath GetPath() const OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool in_memory) OVERRIDE;
  virtual void RequestMIDISysExPermission(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame,
      const MIDISysExPermissionCallback& callback) OVERRIDE;
  virtual void CancelMIDISysExPermissionRequest(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame) OVERRIDE;
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() OVERRIDE;
  virtual content::GeolocationPermissionContext*
      GetGeolocationPermissionContext() OVERRIDE;
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;

 private:
  std::string name_;
  base::FilePath path_;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_BROWSER_CONTEXT_H_
