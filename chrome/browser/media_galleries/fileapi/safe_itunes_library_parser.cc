// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_itunes_library_parser.h"

#include "chrome/browser/media_galleries/fileapi/media_file_system_mount_point_provider.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "ipc/ipc_platform_file.h"

using chrome::MediaFileSystemMountPointProvider;
using content::BrowserThread;
using content::UtilityProcessHost;

namespace itunes {

SafeITunesLibraryParser::SafeITunesLibraryParser(
    const base::FilePath& itunes_library_file,
    const ParserCallback& callback)
    : itunes_library_file_(itunes_library_file),
      callback_(callback),
      callback_called_(false) {}

void SafeITunesLibraryParser::Start() {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());

  // |itunes_library_platform_file_| will be closed on the IO thread once it
  // has been handed off to the child process.
  itunes_library_platform_file_ = base::CreatePlatformFile(
      itunes_library_file_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,   // created
      NULL);  // error_code
  if (itunes_library_platform_file_ == base::kInvalidPlatformFileValue) {
    VLOG(1) << "Could not open iTunes library XML file: "
            << itunes_library_file_.value();
    DoParserCallback(false /* failed */, parser::Library());
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeITunesLibraryParser::StartProcessOnIOThread, this));
}

SafeITunesLibraryParser::~SafeITunesLibraryParser() {}

void SafeITunesLibraryParser::StartProcessOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::MessageLoopProxy* message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get();
  utility_process_host_ =
      UtilityProcessHost::Create(this, message_loop_proxy)->AsWeakPtr();
  // Wait for the startup notification before sending the main IPC to the
  // utility process, so that we can dup the file handle.
  utility_process_host_->Send(new ChromeUtilityMsg_StartupPing);
}

void SafeITunesLibraryParser::OnUtilityProcessStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (utility_process_host_->GetData().handle == base::kNullProcessHandle)
    DLOG(ERROR) << "Child process handle is null";
  utility_process_host_->Send(
      new ChromeUtilityMsg_ParseITunesLibraryXmlFile(
          IPC::GetFileHandleForProcess(
              itunes_library_platform_file_,
              utility_process_host_->GetData().handle,
              true /* close_source_handle */)));
}

void SafeITunesLibraryParser::OnGotITunesLibrary(
    bool result, const parser::Library& library) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  MediaFileSystemMountPointProvider::MediaTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&SafeITunesLibraryParser::DoParserCallback,
                 this, result, library));
}

void SafeITunesLibraryParser::DoParserCallback(
    bool result, const parser::Library& library) {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  if (callback_called_)
    return;
  callback_.Run(result, library);
  callback_called_ = true;
}

void SafeITunesLibraryParser::OnProcessCrashed(int exit_code) {
  OnGotITunesLibrary(false /* failed */, parser::Library());
}

bool SafeITunesLibraryParser::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafeITunesLibraryParser, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ProcessStarted,
                        OnUtilityProcessStarted)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GotITunesLibrary,
                        OnGotITunesLibrary)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace itunes
