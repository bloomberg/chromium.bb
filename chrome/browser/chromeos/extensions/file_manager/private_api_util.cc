// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

using content::BrowserThread;
using google_apis::InstalledApp;

namespace file_manager {
namespace util {
namespace {

// The struct is used for GetSelectedFileInfo().
struct GetSelectedFileInfoParams {
  bool for_opening;
  GetSelectedFileInfoCallback callback;
  std::vector<base::FilePath> file_paths;
  std::vector<ui::SelectedFileInfo> selected_files;
};

// Forward declarations of helper functions for GetSelectedFileInfo().
void ContinueGetSelectedFileInfo(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params,
                                 drive::FileError error,
                                 const base::FilePath& local_file_path,
                                 scoped_ptr<drive::ResourceEntry> entry);

// Part of GetSelectedFileInfo().
void GetSelectedFileInfoInternal(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params) {
  DCHECK(profile);

  for (size_t i = params->selected_files.size();
       i < params->file_paths.size(); ++i) {
    const base::FilePath& file_path = params->file_paths[i];
    // When opening a drive file, we should get local file path.
    if (params->for_opening &&
        drive::util::IsUnderDriveMountPoint(file_path)) {
      drive::DriveIntegrationService* integration_service =
          drive::DriveIntegrationServiceFactory::GetForProfile(profile);
      // |integration_service| is NULL if Drive is disabled.
      if (!integration_service) {
        ContinueGetSelectedFileInfo(profile,
                                    params.Pass(),
                                    drive::FILE_ERROR_FAILED,
                                    base::FilePath(),
                                    scoped_ptr<drive::ResourceEntry>());
        return;
      }
      integration_service->file_system()->GetFileByPath(
          drive::util::ExtractDrivePath(file_path),
          base::Bind(&ContinueGetSelectedFileInfo,
                     profile,
                     base::Passed(&params)));
      return;
    } else {
      params->selected_files.push_back(
          ui::SelectedFileInfo(file_path, base::FilePath()));
    }
  }
  params->callback.Run(params->selected_files);
}

// Part of GetSelectedFileInfo().
void ContinueGetSelectedFileInfo(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params,
                                 drive::FileError error,
                                 const base::FilePath& local_file_path,
                                 scoped_ptr<drive::ResourceEntry> entry) {
  DCHECK(profile);

  const int index = params->selected_files.size();
  const base::FilePath& file_path = params->file_paths[index];
  base::FilePath local_path;
  if (error == drive::FILE_ERROR_OK) {
    local_path = local_file_path;
  } else {
    DLOG(ERROR) << "Failed to get " << file_path.value()
                << " with error code: " << error;
  }
  params->selected_files.push_back(ui::SelectedFileInfo(file_path, local_path));
  GetSelectedFileInfoInternal(profile, params.Pass());
}

}  // namespace

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

void GetSelectedFileInfo(content::RenderViewHost* render_view_host,
                         Profile* profile,
                         const std::vector<GURL>& file_urls,
                         bool for_opening,
                         GetSelectedFileInfoCallback callback) {
  DCHECK(render_view_host);
  DCHECK(profile);

  scoped_ptr<GetSelectedFileInfoParams> params(new GetSelectedFileInfoParams);
  params->for_opening = for_opening;
  params->callback = callback;

  for (size_t i = 0; i < file_urls.size(); ++i) {
    const GURL& file_url = file_urls[i];
    const base::FilePath path = GetLocalPathFromURL(
        render_view_host, profile, file_url);
    if (!path.empty()) {
      DVLOG(1) << "Selected: file path: " << path.value();
      params->file_paths.push_back(path);
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetSelectedFileInfoInternal,
                 profile,
                 base::Passed(&params)));
}

}  // namespace util
}  // namespace file_manager
