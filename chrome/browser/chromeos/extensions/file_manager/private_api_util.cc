// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

using google_apis::InstalledApp;

namespace file_manager {

int32 GetTabId(ExtensionFunctionDispatcher* dispatcher) {
  if (!dispatcher) {
    LOG(WARNING) << "No dispatcher";
    return 0;
  }
  if (!dispatcher->delegate()) {
    LOG(WARNING) << "No delegate";
    return 0;
  }
  content::WebContents* web_contents =
      dispatcher->delegate()->GetAssociatedWebContents();
  if (!web_contents) {
    LOG(WARNING) << "No associated tab contents";
    return 0;
  }
  return ExtensionTabUtil::GetTabId(web_contents);
}

GURL FindPreferredIcon(const InstalledApp::IconList& icons,
                       int preferred_size) {
  GURL result;
  if (icons.empty())
    return result;
  result = icons.rbegin()->second;
  for (InstalledApp::IconList::const_reverse_iterator iter = icons.rbegin();
       iter != icons.rend() && iter->first >= preferred_size; ++iter) {
    result = iter->second;
  }
  return result;
}

base::FilePath GetLocalPathFromURL(
    content::RenderViewHost* render_view_host,
    Profile* profile,
    const GURL& url) {
  DCHECK(render_view_host);
  DCHECK(profile);

  content::SiteInstance* site_instance = render_view_host->GetSiteInstance();
  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      content::BrowserContext::GetStoragePartition(profile, site_instance)->
          GetFileSystemContext();

  const fileapi::FileSystemURL filesystem_url(
      file_system_context->CrackURL(url));
  base::FilePath path;
  if (!chromeos::FileSystemBackend::CanHandleURL(filesystem_url))
    return base::FilePath();
  return filesystem_url.path();
}

}  // namespace file_manager
