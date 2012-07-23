// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/pnacl_file_host.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
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

void NotifyRendererOfError(
    ChromeRenderMessageFilter* chrome_render_message_filter,
    IPC::Message* reply_msg) {
  reply_msg->set_reply_error();
  chrome_render_message_filter->Send(reply_msg);
}

bool PnaclDoOpenFile(const FilePath& file_to_open,
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
  FilePath full_filepath;

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

bool PnaclCanOpenFile(const std::string& filename,
                      FilePath* file_to_open) {
  // The file must use only ASCII characters.
  if (!IsStringASCII(filename)) {
    return false;
  }

  // Disallow special shell characters, just in case...
  if (filename.find('%') != std::string::npos ||
      filename.find('$') != std::string::npos) {
    return false;
  }

#if defined(OS_WIN)
  FilePath file_to_find(ASCIIToUTF16(filename));
#elif defined(OS_POSIX)
  FilePath file_to_find(filename);
#endif

  if (file_to_find.empty() || file_util::IsDot(file_to_find)) {
    return false;
  }

  // Disallow peeking outside of the pnacl component directory.
  if (file_to_find.ReferencesParent() || file_to_find.IsAbsolute()) {
    return false;
  }

  FilePath pnacl_dir;
  if (!PathService::Get(chrome::DIR_PNACL_COMPONENT, &pnacl_dir)) {
    return false;
  }
  if (pnacl_dir.empty()) {
    return false;
  }

  FilePath full_path = pnacl_dir.Append(file_to_find);
  *file_to_open = full_path;
  return true;
}

}  // namespace pnacl_file_host
