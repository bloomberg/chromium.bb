// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_iapps_library_parser.h"

#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "ipc/ipc_platform_file.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace iapps {

SafeIAppsLibraryParser::SafeIAppsLibraryParser()
    : parser_state_(INITIAL_STATE) {}

void SafeIAppsLibraryParser::ParseIPhotoLibrary(
    const base::FilePath& library_file,
    const IPhotoParserCallback& callback) {
  library_file_path_ = library_file;
  iphoto_callback_ = callback;
  Start();
}

void SafeIAppsLibraryParser::ParseITunesLibrary(
    const base::FilePath& library_file,
    const ITunesParserCallback& callback) {
  library_file_path_ = library_file;
  itunes_callback_ = callback;
  Start();
}

void SafeIAppsLibraryParser::Start() {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());

  // |library_file_| will be closed on the IO thread once it has been handed
  // off to the child process.
  library_file_.Initialize(library_file_path_,
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!library_file_.IsValid()) {
    VLOG(1) << "Could not open iApps library XML file: "
            << library_file_path_.value();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeIAppsLibraryParser::OnOpenLibraryFileFailed, this));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeIAppsLibraryParser::StartProcessOnIOThread, this));
}

SafeIAppsLibraryParser::~SafeIAppsLibraryParser() {}

void SafeIAppsLibraryParser::StartProcessOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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

void SafeIAppsLibraryParser::OnUtilityProcessStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (parser_state_ != PINGED_UTILITY_PROCESS_STATE)
    return;

  if (utility_process_host_->GetData().handle == base::kNullProcessHandle) {
    DLOG(ERROR) << "Child process handle is null";
    OnError();
    return;
  }

  if (!itunes_callback_.is_null()) {
    utility_process_host_->Send(
        new ChromeUtilityMsg_ParseITunesLibraryXmlFile(
            IPC::TakeFileHandleForProcess(
                library_file_.Pass(),
                utility_process_host_->GetData().handle)));
  } else if (!iphoto_callback_.is_null()) {
#if defined(OS_MACOSX)
    utility_process_host_->Send(
        new ChromeUtilityMsg_ParseIPhotoLibraryXmlFile(
            IPC::TakeFileHandleForProcess(
                library_file_.Pass(),
                utility_process_host_->GetData().handle)));
#endif
  }

  parser_state_ = STARTED_PARSING_STATE;
}

#if defined(OS_MACOSX)
void SafeIAppsLibraryParser::OnGotIPhotoLibrary(
    bool result, const iphoto::parser::Library& library) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!iphoto_callback_.is_null());

  if (parser_state_ != STARTED_PARSING_STATE)
    return;

  MediaFileSystemBackend::MediaTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(iphoto_callback_, result, library));
  parser_state_ = FINISHED_PARSING_STATE;
}
#endif

void SafeIAppsLibraryParser::OnGotITunesLibrary(
    bool result, const itunes::parser::Library& library) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!itunes_callback_.is_null());

  if (parser_state_ != STARTED_PARSING_STATE)
    return;

  MediaFileSystemBackend::MediaTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(itunes_callback_, result, library));
  parser_state_ = FINISHED_PARSING_STATE;
}

void SafeIAppsLibraryParser::OnOpenLibraryFileFailed() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  OnError();
}

void SafeIAppsLibraryParser::OnProcessCrashed(int exit_code) {
  OnError();
}

void SafeIAppsLibraryParser::OnError() {
  parser_state_ = FINISHED_PARSING_STATE;
  if (!itunes_callback_.is_null())
    OnGotITunesLibrary(false /* failed */, itunes::parser::Library());

#if defined(OS_MACOSX)
  if (!iphoto_callback_.is_null())
    OnGotIPhotoLibrary(false /* failed */, iphoto::parser::Library());
#endif
}

bool SafeIAppsLibraryParser::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafeIAppsLibraryParser, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ProcessStarted,
                        OnUtilityProcessStarted)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GotIPhotoLibrary,
                        OnGotIPhotoLibrary)
#endif
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GotITunesLibrary,
                        OnGotITunesLibrary)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace iapps
