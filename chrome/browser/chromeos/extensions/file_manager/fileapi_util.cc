// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/fileapi_util.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"

namespace file_manager {
namespace fileapi_util {

fileapi::FileSystemContext* GetFileSystemContextForExtensionId(
    Profile* profile,
    const std::string& extension_id) {
  GURL site = extensions::ExtensionSystem::Get(profile)->
      extension_service()->GetSiteForExtensionId(extension_id);
  return content::BrowserContext::GetStoragePartitionForSite(profile, site)->
      GetFileSystemContext();
}

fileapi::FileSystemContext* GetFileSystemContextForRenderViewHost(
    Profile* profile,
    content::RenderViewHost* render_view_host) {
  content::SiteInstance* site_instance = render_view_host->GetSiteInstance();
  return content::BrowserContext::GetStoragePartition(profile, site_instance)->
      GetFileSystemContext();
}

}  // namespace fileapi_util
}  // namespace file_manager
