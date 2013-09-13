// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_itunes_library_parser.h"

#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "ipc/ipc_platform_file.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace itunes {

SafeITunesLibraryParser::SafeITunesLibraryParser(
    const base::FilePath& itunes_library_file,
    const ParserCallback& callback)
    : itunes_library_file_(itunes_library_file),
      callback_(callback),
      parser_state_(INITIAL_STATE) {}

void SafeITunesLibraryParser::Start() {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());

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
    callback_.Run(false /* failed */, parser::Library());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeITunesLibraryParser::OnOpenLibraryFileFailed, this));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeITunesLibraryParser::StartProcessOnIOThread, this));
}

SafeITunesLibraryParser::~SafeITunesLibraryParser() {}

void SafeITunesLibraryParser::StartProcessOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(INITIAL_STATE, parser_state_);

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  utility_process_host_ =
      UtilityProcessHost::Create(this, message_loop_proxy.get())->AsWeakPtr();
  // Wait for the startup notification before sending the main IPC to the
  // utility process, so that we can dup the file handle.
  utility_process_host_->Send(new ChromeUtilityMsg_StartupPing);
  parser_state_ = PINGED_UTILITY_PROCESS_STATE;
}

void SafeITunesLibraryParser::OnUtilityProcessStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (parser_state_ != PINGED_UTILITY_PROCESS_STATE)
    return;

  if (utility_process_host_->GetData().handle == base::kNullProcessHandle)
    DLOG(ERROR) << "Child process handle is null";
  utility_process_host_->Send(
      new ChromeUtilityMsg_ParseITunesLibraryXmlFile(
          IPC::GetFileHandleForProcess(
              itunes_library_platform_file_,
              utility_process_host_->GetData().handle,
              true /* close_source_handle */)));
  parser_state_ = STARTED_PARSING_STATE;
}

void SafeITunesLibraryParser::OnGotITunesLibrary(
    bool result, const parser::Library& library) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (parser_state_ != STARTED_PARSING_STATE)
    return;

  MediaFileSystemBackend::MediaTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(callback_, result, library));
  parser_state_ = FINISHED_PARSING_STATE;
}

void SafeITunesLibraryParser::OnOpenLibraryFileFailed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  parser_state_ = FINISHED_PARSING_STATE;
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
