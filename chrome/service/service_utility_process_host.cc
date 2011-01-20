// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_utility_process_host.h"

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/utility_messages.h"
#include "gfx/rect.h"
#include "ipc/ipc_switches.h"
#include "printing/native_metafile.h"
#include "printing/page_range.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

ServiceUtilityProcessHost::ServiceUtilityProcessHost(
    Client* client, base::MessageLoopProxy* client_message_loop_proxy)
        : ServiceChildProcessHost(ChildProcessInfo::UTILITY_PROCESS),
          client_(client),
          client_message_loop_proxy_(client_message_loop_proxy),
          waiting_for_reply_(false) {
}

ServiceUtilityProcessHost::~ServiceUtilityProcessHost() {
}

bool ServiceUtilityProcessHost::StartRenderPDFPagesToMetafile(
    const FilePath& pdf_path,
    const gfx::Rect& render_area,
    int render_dpi,
    const std::vector<printing::PageRange>& page_ranges) {
#if !defined(OS_WIN)
  // This is only implemented on Windows (because currently it is only needed
  // on Windows). Will add implementations on other platforms when needed.
  NOTIMPLEMENTED();
  return false;
#else  // !defined(OS_WIN)
  scratch_metafile_dir_.reset(new ScopedTempDir);
  if (!scratch_metafile_dir_->CreateUniqueTempDir())
    return false;
  if (!file_util::CreateTemporaryFileInDir(scratch_metafile_dir_->path(),
                                           &metafile_path_)) {
    return false;
  }

  if (!StartProcess(false, scratch_metafile_dir_->path()))
    return false;

  base::win::ScopedHandle pdf_file(
      ::CreateFile(pdf_path.value().c_str(),
                   GENERIC_READ,
                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                   NULL,
                   OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL,
                   NULL));
  if (pdf_file == INVALID_HANDLE_VALUE)
    return false;
  HANDLE pdf_file_in_utility_process = NULL;
  ::DuplicateHandle(::GetCurrentProcess(), pdf_file, handle(),
                    &pdf_file_in_utility_process, 0, false,
                    DUPLICATE_SAME_ACCESS);
  if (!pdf_file_in_utility_process)
    return false;
  waiting_for_reply_ = true;
  return Send(
      new UtilityMsg_RenderPDFPagesToMetafile(pdf_file_in_utility_process,
                                              metafile_path_,
                                              render_area,
                                              render_dpi,
                                              page_ranges));
#endif  // !defined(OS_WIN)
}

bool ServiceUtilityProcessHost::StartGetPrinterCapsAndDefaults(
    const std::string& printer_name) {
  FilePath exposed_path;
  if (!StartProcess(true, exposed_path))
    return false;
  waiting_for_reply_ = true;
  return Send(new UtilityMsg_GetPrinterCapsAndDefaults(printer_name));
}

bool ServiceUtilityProcessHost::StartProcess(bool no_sandbox,
                                             const FilePath& exposed_dir) {
  // Name must be set or metrics_service will crash in any test which
  // launches a UtilityProcessHost.
  set_name(L"utility process");

  if (!CreateChannel())
    return false;

  FilePath exe_path = GetUtilityProcessCmd();
  if (exe_path.empty()) {
    NOTREACHED() << "Unable to get utility process binary name.";
    return false;
  }

  CommandLine cmd_line(exe_path);
  cmd_line.AppendSwitchASCII(switches::kProcessType, switches::kUtilityProcess);
  cmd_line.AppendSwitchASCII(switches::kProcessChannelID, channel_id());
  cmd_line.AppendSwitch(switches::kLang);

  return Launch(&cmd_line, no_sandbox, exposed_dir);
}

FilePath ServiceUtilityProcessHost::GetUtilityProcessCmd() {
  return GetChildPath(true);
}

bool ServiceUtilityProcessHost::CanShutdown() {
  return true;
}

void ServiceUtilityProcessHost::OnChildDied() {
  if (waiting_for_reply_) {
    // If we are yet to receive a reply then notify the client that the
    // child died.
    client_message_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(client_.get(), &Client::OnChildDied));
  }
  // The base class implementation will delete |this|.
  ServiceChildProcessHost::OnChildDied();
}

bool ServiceUtilityProcessHost::OnMessageReceived(const IPC::Message& message) {
  bool msg_is_ok = false;
  IPC_BEGIN_MESSAGE_MAP_EX(ServiceUtilityProcessHost, message, msg_is_ok)
#if defined(OS_WIN)  // This hack is Windows-specific.
    IPC_MESSAGE_HANDLER(UtilityHostMsg_PreCacheFont, OnPreCacheFont)
#endif
    IPC_MESSAGE_HANDLER(UtilityHostMsg_RenderPDFPagesToMetafile_Succeeded,
                        OnRenderPDFPagesToMetafileSucceeded)
    IPC_MESSAGE_UNHANDLED(msg_is_ok__ = MessageForClient(message))
  IPC_END_MESSAGE_MAP_EX()
  return true;
}

bool ServiceUtilityProcessHost::MessageForClient(const IPC::Message& message) {
  DCHECK(waiting_for_reply_);
  bool ret = client_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(client_.get(), &Client::OnMessageReceived,
                        message));
  waiting_for_reply_ = false;
  return ret;
}

#if defined(OS_WIN)  // This hack is Windows-specific.
void ServiceUtilityProcessHost::OnPreCacheFont(LOGFONT font) {
  PreCacheFont(font);
}
#endif  // OS_WIN

void ServiceUtilityProcessHost::OnRenderPDFPagesToMetafileSucceeded(
    int highest_rendered_page_number) {
  DCHECK(waiting_for_reply_);
  // If the metafile was successfully created, we need to take our hands off the
  // scratch metafile directory. The client will delete it when it is done with
  // metafile.
  scratch_metafile_dir_->Take();
  client_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(client_.get(),
                        &Client::MetafileAvailable,
                        metafile_path_,
                        highest_rendered_page_number));
  waiting_for_reply_ = false;
}

bool ServiceUtilityProcessHost::Client::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceUtilityProcessHost, message)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_RenderPDFPagesToMetafile_Failed,
                        Client::OnRenderPDFPagesToMetafileFailed)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_GetPrinterCapsAndDefaults_Succeeded,
                        Client::OnGetPrinterCapsAndDefaultsSucceeded)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_GetPrinterCapsAndDefaults_Failed,
                        Client::OnGetPrinterCapsAndDefaultsFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void ServiceUtilityProcessHost::Client::MetafileAvailable(
    const FilePath& metafile_path,
    int highest_rendered_page_number) {
  // The metafile was created in a temp folder which needs to get deleted after
  // we have processed it.
  ScopedTempDir scratch_metafile_dir;
  if (!scratch_metafile_dir.Set(metafile_path.DirName()))
    LOG(WARNING) << "Unable to set scratch metafile directory";
#if defined(OS_WIN)
  printing::NativeMetafile metafile;
  if (!metafile.CreateFromFile(metafile_path)) {
    OnRenderPDFPagesToMetafileFailed();
  } else {
    OnRenderPDFPagesToMetafileSucceeded(metafile, highest_rendered_page_number);
    // Close it so that ScopedTempDir can delete the folder.
    metafile.CloseEmf();
  }
#endif  // defined(OS_WIN)
}

