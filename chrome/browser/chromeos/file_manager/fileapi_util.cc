// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/fileapi_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/escape.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace file_manager {
namespace util {

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

GURL ConvertRelativeFilePathToFileSystemUrl(const base::FilePath& relative_path,
                                            const std::string& extension_id) {
  GURL base_url = fileapi::GetFileSystemRootURI(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id),
      fileapi::kFileSystemTypeExternal);
  return GURL(base_url.spec() +
              net::EscapeUrlEncodedData(relative_path.AsUTF8Unsafe(),
                                        false));  // Space to %20 instead of +.
}

bool ConvertAbsoluteFilePathToFileSystemUrl(
    Profile* profile,
    const base::FilePath& absolute_path,
    const std::string& extension_id,
    GURL* url) {
  base::FilePath relative_path;
  if (!ConvertAbsoluteFilePathToRelativeFileSystemPath(
          profile,
          extension_id,
          absolute_path,
          &relative_path)) {
    return false;
  }
  *url = ConvertRelativeFilePathToFileSystemUrl(relative_path, extension_id);
  return true;
}

bool ConvertAbsoluteFilePathToRelativeFileSystemPath(
    Profile* profile,
    const std::string& extension_id,
    const base::FilePath& absolute_path,
    base::FilePath* virtual_path) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  // May be NULL during unit_tests.
  if (!service)
    return false;

  // File browser APIs are meant to be used only from extension context, so the
  // extension's site is the one in whose file system context the virtual path
  // should be found.
  GURL site = service->GetSiteForExtensionId(extension_id);
  fileapi::ExternalFileSystemBackend* backend =
      content::BrowserContext::GetStoragePartitionForSite(profile, site)->
          GetFileSystemContext()->external_backend();
  if (!backend)
    return false;

  // Find if this file path is managed by the external backend.
  if (!backend->GetVirtualPath(absolute_path, virtual_path))
    return false;

  return true;
}

}  // namespace util
}  // namespace file_manager
