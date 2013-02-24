// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/pnacl_file_host.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_platform_file.h"

using content::BrowserThread;

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
    ChromeRenderMessageFilter* chrome_render_message_filter,
    const std::string& filename,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath full_filepath;

  // Do some validation.
  if (!pnacl_file_host::PnaclCanOpenFile(filename, &full_filepath)) {
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
    ChromeRenderMessageFilter* chrome_render_message_filter,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

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

}  // namespace

namespace pnacl_file_host {

void GetReadonlyPnaclFd(
    ChromeRenderMessageFilter* chrome_render_message_filter,
    const std::string& filename,
    IPC::Message* reply_msg) {
  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&DoOpenPnaclFile,
                     make_scoped_refptr(chrome_render_message_filter),
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
    ChromeRenderMessageFilter* chrome_render_message_filter,
    IPC::Message* reply_msg) {
  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&DoCreateTemporaryFile,
                     make_scoped_refptr(chrome_render_message_filter),
                     reply_msg))) {
    NotifyRendererOfError(chrome_render_message_filter, reply_msg);
  }
}

}  // namespace pnacl_file_host
