// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_file_host.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/nacl_host/nacl_host_message_filter.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/common/nacl_browser_delegate.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "ipc/ipc_platform_file.h"

using content::BrowserThread;

namespace {

// Force a prefix to prevent user from opening "magic" files.
const char* kExpectedFilePrefix = "pnacl_public_";

// Restrict PNaCl file lengths to reduce likelyhood of hitting bugs
// in file name limit error-handling-code-paths, etc.
const size_t kMaxFileLength = 40;

void NotifyRendererOfError(
    NaClHostMessageFilter* nacl_host_message_filter,
    IPC::Message* reply_msg) {
  reply_msg->set_reply_error();
  nacl_host_message_filter->Send(reply_msg);
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
    scoped_refptr<NaClHostMessageFilter> nacl_host_message_filter,
    const std::string& filename,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  base::FilePath full_filepath;

  // PNaCl must be installed.
  base::FilePath pnacl_dir;
  if (!nacl::NaClBrowser::GetDelegate()->GetPnaclDirectory(&pnacl_dir) ||
      !base::PathExists(pnacl_dir)) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }

  // Do some validation.
  if (!nacl_file_host::PnaclCanOpenFile(filename, &full_filepath)) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }

  base::PlatformFile file_to_open;
  if (!PnaclDoOpenFile(full_filepath, &file_to_open)) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }

  // Send the reply!
  // Do any DuplicateHandle magic that is necessary first.
  IPC::PlatformFileForTransit target_desc =
      IPC::GetFileHandleForProcess(file_to_open,
                                   nacl_host_message_filter->PeerHandle(),
                                   true /* Close source */);
  if (target_desc == IPC::InvalidPlatformFileForTransit()) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }
  NaClHostMsg_GetReadonlyPnaclFD::WriteReplyParams(
      reply_msg, target_desc);
  nacl_host_message_filter->Send(reply_msg);
}

void DoRegisterOpenedNaClExecutableFile(
    scoped_refptr<NaClHostMessageFilter> nacl_host_message_filter,
    base::PlatformFile file,
    base::FilePath file_path,
    IPC::Message* reply_msg) {
  // IO thread owns the NaClBrowser singleton.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  nacl::NaClBrowser* nacl_browser = nacl::NaClBrowser::GetInstance();
  uint64 file_token_lo = 0;
  uint64 file_token_hi = 0;
  nacl_browser->PutFilePath(file_path, &file_token_lo, &file_token_hi);

  IPC::PlatformFileForTransit file_desc = IPC::GetFileHandleForProcess(
      file,
      nacl_host_message_filter->PeerHandle(),
      true /* close_source */);

  NaClHostMsg_OpenNaClExecutable::WriteReplyParams(
      reply_msg, file_desc, file_token_lo, file_token_hi);
  nacl_host_message_filter->Send(reply_msg);
}

// Convert the file URL into a file descriptor.
// This function is security sensitive.  Be sure to check with a security
// person before you modify it.
void DoOpenNaClExecutableOnThreadPool(
    scoped_refptr<NaClHostMessageFilter> nacl_host_message_filter,
    const GURL& file_url,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  base::FilePath file_path;
  if (!nacl::NaClBrowser::GetDelegate()->MapUrlToLocalFilePath(
          file_url, true /* use_blocking_api */, &file_path)) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }

  base::PlatformFile file;
  nacl::OpenNaClExecutableImpl(file_path, &file);
  if (file != base::kInvalidPlatformFileValue) {
    // This function is running on the blocking pool, but the path needs to be
    // registered in a structure owned by the IO thread.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &DoRegisterOpenedNaClExecutableFile,
            nacl_host_message_filter,
            file, file_path, reply_msg));
  } else {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }
}

}  // namespace

namespace nacl_file_host {

void GetReadonlyPnaclFd(
    scoped_refptr<NaClHostMessageFilter> nacl_host_message_filter,
    const std::string& filename,
    IPC::Message* reply_msg) {
  if (!BrowserThread::PostBlockingPoolTask(
          FROM_HERE,
          base::Bind(&DoOpenPnaclFile,
                     nacl_host_message_filter,
                     filename,
                     reply_msg))) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
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
  if (!nacl::NaClBrowser::GetDelegate()->GetPnaclDirectory(&pnacl_dir) ||
      pnacl_dir.empty())
    return false;

  // Prepend the prefix to restrict files to a whitelisted set.
  base::FilePath full_path = pnacl_dir.AppendASCII(
      std::string(kExpectedFilePrefix) + filename);
  *file_to_open = full_path;
  return true;
}

void OpenNaClExecutable(
    scoped_refptr<NaClHostMessageFilter> nacl_host_message_filter,
    int render_view_id,
    const GURL& file_url,
    IPC::Message* reply_msg) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &OpenNaClExecutable,
            nacl_host_message_filter,
            render_view_id, file_url, reply_msg));
    return;
  }

  // Make sure render_view_id is valid and that the URL is a part of the
  // render view's site. Without these checks, apps could probe the extension
  // directory or run NaCl code from other extensions.
  content::RenderViewHost* rvh = content::RenderViewHost::FromID(
      nacl_host_message_filter->render_process_id(), render_view_id);
  if (!rvh) {
    nacl_host_message_filter->BadMessageReceived();  // Kill the renderer.
    return;
  }
  content::SiteInstance* site_instance = rvh->GetSiteInstance();
  if (!content::SiteInstance::IsSameWebSite(site_instance->GetBrowserContext(),
                                            site_instance->GetSiteURL(),
                                            file_url)) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }

  // The URL is part of the current app. Now query the extension system for the
  // file path and convert that to a file descriptor. This should be done on a
  // blocking pool thread.
  if (!BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(
          &DoOpenNaClExecutableOnThreadPool,
          nacl_host_message_filter,
          file_url, reply_msg))) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
  }
}

}  // namespace nacl_file_host
