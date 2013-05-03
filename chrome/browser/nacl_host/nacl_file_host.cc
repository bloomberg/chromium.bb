// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_file_host.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/manifest_handlers/shared_module_info.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "ipc/ipc_platform_file.h"

using content::BrowserThread;
using extensions::SharedModuleInfo;

namespace {

// Force a prefix to prevent user from opening "magic" files.
const char* kExpectedFilePrefix = "pnacl_public_";

// Restrict PNaCl file lengths to reduce likelyhood of hitting bugs
// in file name limit error-handling-code-paths, etc.
const size_t kMaxFileLength = 40;

void NotifyRendererOfError(
    ChromeRenderMessageFilter* chrome_render_message_filter,
    IPC::Message* reply_msg) {
  reply_msg->set_reply_error();
  chrome_render_message_filter->Send(reply_msg);
}

bool PnaclDoOpenFile(const base::FilePath& file_to_open,
                     base::PlatformFile* out_file) {
  base::PlatformFileError error_code;
  *out_file = base::CreatePlatformFile(file_to_open,
                                       base::PLATFORM_FILE_OPEN |
                                       base::PLATFORM_FILE_READ,
                                       NULL,
                                       &error_code);
  if (error_code != base::PLATFORM_FILE_OK) {
    return false;
  }
  return true;
}

void DoOpenPnaclFile(
    scoped_refptr<ChromeRenderMessageFilter> chrome_render_message_filter,
    const std::string& filename,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  base::FilePath full_filepath;

  // Do some validation.
  if (!nacl_file_host::PnaclCanOpenFile(filename, &full_filepath)) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
    return;
  }

  base::PlatformFile file_to_open;
  if (!PnaclDoOpenFile(full_filepath, &file_to_open)) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
    return;
  }

  // Send the reply!
  // Do any DuplicateHandle magic that is necessary first.
  IPC::PlatformFileForTransit target_desc =
      IPC::GetFileHandleForProcess(file_to_open,
                                   chrome_render_message_filter->peer_handle(),
                                   true /* Close source */);
  if (target_desc == IPC::InvalidPlatformFileForTransit()) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
    return;
  }
  ChromeViewHostMsg_GetReadonlyPnaclFD::WriteReplyParams(
      reply_msg, target_desc);
  chrome_render_message_filter->Send(reply_msg);
}

void DoCreateTemporaryFile(
    scoped_refptr<ChromeRenderMessageFilter> chrome_render_message_filter,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  base::FilePath file_path;
  if (!file_util::CreateTemporaryFile(&file_path)) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
    return;
  }

  base::PlatformFileError error;
  base::PlatformFile file_handle = base::CreatePlatformFile(
      file_path,
      base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_TEMPORARY |
      base::PLATFORM_FILE_DELETE_ON_CLOSE,
      NULL, &error);

  if (error != base::PLATFORM_FILE_OK) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
    return;
  }

  // Send the reply!
  // Do any DuplicateHandle magic that is necessary first.
  IPC::PlatformFileForTransit target_desc =
      IPC::GetFileHandleForProcess(file_handle,
                                   chrome_render_message_filter->peer_handle(),
                                   true);
  if (target_desc == IPC::InvalidPlatformFileForTransit()) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
    return;
  }

  ChromeViewHostMsg_NaClCreateTemporaryFile::WriteReplyParams(
      reply_msg, target_desc);
  chrome_render_message_filter->Send(reply_msg);
}

// Convert the file URL into a file path in the extension directory.
// This function is security sensitive.  Be sure to check with a security
// person before you modify it.
bool GetExtensionFilePath(
    scoped_refptr<ExtensionInfoMap> extension_info_map,
    const GURL& file_url,
    base::FilePath* file_path) {
  // Check that the URL is recognized by the extension system.
  const extensions::Extension* extension =
      extension_info_map->extensions().GetExtensionOrAppByURL(
          ExtensionURLInfo(file_url));
  if (!extension)
    return false;

  std::string path = file_url.path();
  extensions::ExtensionResource resource;

  if (SharedModuleInfo::IsImportedPath(path)) {
    // Check if this is a valid path that is imported for this extension.
    std::string new_extension_id;
    std::string new_relative_path;
    SharedModuleInfo::ParseImportedPath(path, &new_extension_id,
                                        &new_relative_path);
    const extensions::Extension* new_extension =
        extension_info_map->extensions().GetByID(new_extension_id);
    if (!new_extension)
      return false;

    if (!SharedModuleInfo::ImportsExtensionById(extension, new_extension_id) ||
        !SharedModuleInfo::IsExportAllowed(new_extension, new_relative_path)) {
      return false;
    }

    resource = new_extension->GetResource(new_relative_path);
  } else {
    // Check that the URL references a resource in the extension.
    resource = extension->GetResource(path);
  }

  if (resource.empty())
    return false;

  const base::FilePath resource_file_path = resource.GetFilePath();
  if (resource_file_path.empty())
    return false;

  *file_path = resource_file_path;
  return true;
}

// Convert the file URL into a file descriptor.
// This function is security sensitive.  Be sure to check with a security
// person before you modify it.
void DoOpenNaClExecutableOnThreadPool(
    scoped_refptr<ChromeRenderMessageFilter> chrome_render_message_filter,
    scoped_refptr<ExtensionInfoMap> extension_info_map,
    const GURL& file_url,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  base::FilePath file_path;
  if (!GetExtensionFilePath(extension_info_map, file_url, &file_path)) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
    return;
  }

  // Get a file descriptor. On Windows, we need 'GENERIC_EXECUTE' in order to
  // memory map the executable.
  // IMPORTANT: This file descriptor must not have write access - that could
  // allow a sandbox escape.
  base::PlatformFileError error_code;
  base::PlatformFile file = base::CreatePlatformFile(
      file_path,
      base::PLATFORM_FILE_OPEN |
          base::PLATFORM_FILE_READ |
          base::PLATFORM_FILE_EXECUTE,  // Windows only flag.
      NULL,
      &error_code);
  if (error_code != base::PLATFORM_FILE_OK) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
    return;
  }
  // Check that the file does not reference a directory. Returning a descriptor
  // to an extension directory could allow a sandbox escape.
  base::PlatformFileInfo file_info;
  if (!base::GetPlatformFileInfo(file, &file_info) || file_info.is_directory)
  {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
    return;
  }

  IPC::PlatformFileForTransit file_desc = IPC::GetFileHandleForProcess(
      file,
      chrome_render_message_filter->peer_handle(),
      true /* close_source */);

  ChromeViewHostMsg_OpenNaClExecutable::WriteReplyParams(
      reply_msg, file_path, file_desc);
  chrome_render_message_filter->Send(reply_msg);
}

}  // namespace

namespace nacl_file_host {

void GetReadonlyPnaclFd(
    scoped_refptr<ChromeRenderMessageFilter> chrome_render_message_filter,
    const std::string& filename,
    IPC::Message* reply_msg) {
  if (!BrowserThread::PostBlockingPoolTask(
          FROM_HERE,
          base::Bind(&DoOpenPnaclFile,
                     chrome_render_message_filter,
                     filename,
                     reply_msg))) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
  }
}

// This function is security sensitive.  Be sure to check with a security
// person before you modify it.
bool PnaclCanOpenFile(const std::string& filename,
                      base::FilePath* file_to_open) {
  if (filename.length() > kMaxFileLength)
    return false;

  if (filename.empty())
    return false;

  // Restrict character set of the file name to something really simple
  // (a-z, 0-9, and underscores).
  for (size_t i = 0; i < filename.length(); ++i) {
    char charAt = filename[i];
    if (charAt < 'a' || charAt > 'z')
      if (charAt < '0' || charAt > '9')
        if (charAt != '_')
          return false;
  }

  // PNaCl must be installed.
  base::FilePath pnacl_dir;
  if (!PathService::Get(chrome::DIR_PNACL_COMPONENT, &pnacl_dir) ||
      pnacl_dir.empty())
    return false;

  // Prepend the prefix to restrict files to a whitelisted set.
  base::FilePath full_path = pnacl_dir.AppendASCII(
      std::string(kExpectedFilePrefix) + filename);
  *file_to_open = full_path;
  return true;
}

void CreateTemporaryFile(
    scoped_refptr<ChromeRenderMessageFilter> chrome_render_message_filter,
    IPC::Message* reply_msg) {
  if (!BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&DoCreateTemporaryFile,
                 chrome_render_message_filter,
                 reply_msg))) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
  }
}

void OpenNaClExecutable(
    scoped_refptr<ChromeRenderMessageFilter> chrome_render_message_filter,
    scoped_refptr<ExtensionInfoMap> extension_info_map,
    int render_view_id,
    const GURL& file_url,
    IPC::Message* reply_msg) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &OpenNaClExecutable,
            chrome_render_message_filter,
            extension_info_map,
            render_view_id, file_url, reply_msg));
    return;
  }

  // Make sure render_view_id is valid and that the URL is a part of the
  // render view's site. Without these checks, apps could probe the extension
  // directory or run NaCl code from other extensions.
  content::RenderViewHost* rvh = content::RenderViewHost::FromID(
      chrome_render_message_filter->render_process_id(), render_view_id);
  if (!rvh) {
    chrome_render_message_filter->BadMessageReceived();  // Kill the renderer.
    return;
  }
  content::SiteInstance* site_instance = rvh->GetSiteInstance();
  if (!content::SiteInstance::IsSameWebSite(site_instance->GetBrowserContext(),
                                            site_instance->GetSiteURL(),
                                            file_url)) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
    return;
  }

  // The URL is part of the current app. Now query the extension system for the
  // file path and convert that to a file descriptor. This should be done on a
  // blocking pool thread.
  if (!BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(
          &DoOpenNaClExecutableOnThreadPool,
          chrome_render_message_filter,
          extension_info_map,
          file_url, reply_msg))) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
  }
}

}  // namespace nacl_file_host
