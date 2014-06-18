// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_utility_process_host.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/process/kill.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_utility_printing_messages.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_init.h"
#include "ipc/ipc_switches.h"
#include "printing/page_range.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/rect.h"

#if defined(OS_WIN)
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/launch.h"
#include "base/win/scoped_handle.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "printing/emf_win.h"

namespace {

// NOTE: changes to this class need to be reviewed by the security team.
class ServiceSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  explicit ServiceSandboxedProcessLauncherDelegate(
      const base::FilePath& exposed_dir)
    : exposed_dir_(exposed_dir) {
  }

  virtual void PreSandbox(bool* disable_default_policy,
                          base::FilePath* exposed_dir) OVERRIDE {
    *exposed_dir = exposed_dir_;
  }

 private:
  base::FilePath exposed_dir_;
};

}  // namespace

#endif  // OS_WIN

using content::ChildProcessHost;

namespace {
enum ServiceUtilityProcessHostEvent {
  SERVICE_UTILITY_STARTED,
  SERVICE_UTILITY_DISCONNECTED,
  SERVICE_UTILITY_METAFILE_REQUEST,
  SERVICE_UTILITY_METAFILE_SUCCEEDED,
  SERVICE_UTILITY_METAFILE_FAILED,
  SERVICE_UTILITY_CAPS_REQUEST,
  SERVICE_UTILITY_CAPS_SUCCEEDED,
  SERVICE_UTILITY_CAPS_FAILED,
  SERVICE_UTILITY_SEMANTIC_CAPS_REQUEST,
  SERVICE_UTILITY_SEMANTIC_CAPS_SUCCEEDED,
  SERVICE_UTILITY_SEMANTIC_CAPS_FAILED,
  SERVICE_UTILITY_EVENT_MAX,
};
}  // namespace

ServiceUtilityProcessHost::ServiceUtilityProcessHost(
    Client* client, base::MessageLoopProxy* client_message_loop_proxy)
        : handle_(base::kNullProcessHandle),
          client_(client),
          client_message_loop_proxy_(client_message_loop_proxy),
          waiting_for_reply_(false) {
  child_process_host_.reset(ChildProcessHost::Create(this));
}

ServiceUtilityProcessHost::~ServiceUtilityProcessHost() {
  // We need to kill the child process when the host dies.
  base::KillProcess(handle_, content::RESULT_CODE_NORMAL_EXIT, false);
}

bool ServiceUtilityProcessHost::StartRenderPDFPagesToMetafile(
    const base::FilePath& pdf_path,
    const printing::PdfRenderSettings& render_settings,
    const std::vector<printing::PageRange>& page_ranges) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                            SERVICE_UTILITY_METAFILE_REQUEST,
                            SERVICE_UTILITY_EVENT_MAX);
  start_time_ = base::Time::Now();
#if !defined(OS_WIN)
  // This is only implemented on Windows (because currently it is only needed
  // on Windows). Will add implementations on other platforms when needed.
  NOTIMPLEMENTED();
  return false;
#else  // !defined(OS_WIN)
  scratch_metafile_dir_.reset(new base::ScopedTempDir);
  if (!scratch_metafile_dir_->CreateUniqueTempDir())
    return false;
  metafile_path_ = scratch_metafile_dir_->path().AppendASCII("output.emf");
  if (!StartProcess(false, scratch_metafile_dir_->path()))
    return false;

  base::File pdf_file(
      pdf_path,
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;
  return child_process_host_->Send(
      new ChromeUtilityMsg_RenderPDFPagesToMetafiles(
          IPC::TakeFileHandleForProcess(pdf_file.Pass(), handle()),
          metafile_path_,
          render_settings,
          page_ranges));
#endif  // !defined(OS_WIN)
}

bool ServiceUtilityProcessHost::StartGetPrinterCapsAndDefaults(
    const std::string& printer_name) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                            SERVICE_UTILITY_CAPS_REQUEST,
                            SERVICE_UTILITY_EVENT_MAX);
  start_time_ = base::Time::Now();
  base::FilePath exposed_path;
  if (!StartProcess(true, exposed_path))
    return false;
  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;
  return child_process_host_->Send(
      new ChromeUtilityMsg_GetPrinterCapsAndDefaults(printer_name));
}

bool ServiceUtilityProcessHost::StartGetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                            SERVICE_UTILITY_SEMANTIC_CAPS_REQUEST,
                            SERVICE_UTILITY_EVENT_MAX);
  start_time_ = base::Time::Now();
  base::FilePath exposed_path;
  if (!StartProcess(true, exposed_path))
    return false;
  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;
  return child_process_host_->Send(
      new ChromeUtilityMsg_GetPrinterSemanticCapsAndDefaults(printer_name));
}

bool ServiceUtilityProcessHost::StartProcess(
    bool no_sandbox,
    const base::FilePath& exposed_dir) {
  std::string channel_id = child_process_host_->CreateChannel();
  if (channel_id.empty())
    return false;

  base::FilePath exe_path = GetUtilityProcessCmd();
  if (exe_path.empty()) {
    NOTREACHED() << "Unable to get utility process binary name.";
    return false;
  }

  CommandLine cmd_line(exe_path);
  cmd_line.AppendSwitchASCII(switches::kProcessType, switches::kUtilityProcess);
  cmd_line.AppendSwitchASCII(switches::kProcessChannelID, channel_id);
  cmd_line.AppendSwitch(switches::kLang);

  if (Launch(&cmd_line, no_sandbox, exposed_dir)) {
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                              SERVICE_UTILITY_STARTED,
                              SERVICE_UTILITY_EVENT_MAX);
    return true;
  }
  return false;
}

bool ServiceUtilityProcessHost::Launch(CommandLine* cmd_line,
                                       bool no_sandbox,
                                       const base::FilePath& exposed_dir) {
#if !defined(OS_WIN)
  // TODO(sanjeevr): Implement for non-Windows OSes.
  NOTIMPLEMENTED();
  return false;
#else  // !defined(OS_WIN)

  if (no_sandbox) {
    base::ProcessHandle process = base::kNullProcessHandle;
    cmd_line->AppendSwitch(switches::kNoSandbox);
    base::LaunchProcess(*cmd_line, base::LaunchOptions(), &handle_);
  } else {
    ServiceSandboxedProcessLauncherDelegate delegate(exposed_dir);
    handle_ = content::StartSandboxedProcess(&delegate, cmd_line);
  }
  return (handle_ != base::kNullProcessHandle);
#endif  // !defined(OS_WIN)
}

base::FilePath ServiceUtilityProcessHost::GetUtilityProcessCmd() {
#if defined(OS_LINUX)
  int flags = ChildProcessHost::CHILD_ALLOW_SELF;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif
  return ChildProcessHost::GetChildPath(flags);
}

void ServiceUtilityProcessHost::OnChildDisconnected() {
  if (waiting_for_reply_) {
    // If we are yet to receive a reply then notify the client that the
    // child died.
    client_message_loop_proxy_->PostTask(
        FROM_HERE, base::Bind(&Client::OnChildDied, client_.get()));
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                              SERVICE_UTILITY_DISCONNECTED,
                              SERVICE_UTILITY_EVENT_MAX);
    UMA_HISTOGRAM_TIMES("CloudPrint.ServiceUtilityDisconnectTime",
                        base::Time::Now() - start_time_);
  }
  delete this;
}

bool ServiceUtilityProcessHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceUtilityProcessHost, message)
#if defined(WIN_PDF_METAFILE_FOR_PRINTING)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_RenderPDFPagesToMetafiles_Succeeded,
        OnRenderPDFPagesToMetafilesSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_RenderPDFPagesToMetafile_Failed,
                        OnRenderPDFPagesToMetafileFailed)
#endif
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Succeeded,
        OnGetPrinterCapsAndDefaultsSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Failed,
                        OnGetPrinterCapsAndDefaultsFailed)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Succeeded,
        OnGetPrinterSemanticCapsAndDefaultsSucceeded)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Failed,
        OnGetPrinterSemanticCapsAndDefaultsFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

base::ProcessHandle ServiceUtilityProcessHost::GetHandle() const {
  return handle_;
}

#if defined(WIN_PDF_METAFILE_FOR_PRINTING)
void ServiceUtilityProcessHost::OnRenderPDFPagesToMetafilesSucceeded(
    const std::vector<printing::PageRange>& page_ranges,
    double scale_factor) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                            SERVICE_UTILITY_METAFILE_SUCCEEDED,
                            SERVICE_UTILITY_EVENT_MAX);
  UMA_HISTOGRAM_TIMES("CloudPrint.ServiceUtilityMetafileTime",
                      base::Time::Now() - start_time_);
  DCHECK(waiting_for_reply_);
  waiting_for_reply_ = false;
  // If the metafile was successfully created, we need to take our hands off the
  // scratch metafile directory. The client will delete it when it is done with
  // metafile.
  scratch_metafile_dir_->Take();

  // TODO(vitalybuka|scottmg): http://crbug.com/170859: Currently, only one
  // page is printed at a time. This would need to be refactored to change
  // this.
  CHECK_EQ(1u, page_ranges.size());
  CHECK_EQ(page_ranges[0].from, page_ranges[0].to);
  int page_number = page_ranges[0].from;
  client_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&Client::MetafileAvailable,
                 client_.get(),
                 metafile_path_.InsertBeforeExtensionASCII(
                     base::StringPrintf(".%d", page_number)),
                 page_number,
                 scale_factor));
}

void ServiceUtilityProcessHost::OnRenderPDFPagesToMetafileFailed() {
  DCHECK(waiting_for_reply_);
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                            SERVICE_UTILITY_METAFILE_FAILED,
                            SERVICE_UTILITY_EVENT_MAX);
  UMA_HISTOGRAM_TIMES("CloudPrint.ServiceUtilityMetafileFailTime",
                      base::Time::Now() - start_time_);
  waiting_for_reply_ = false;
  client_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&Client::OnRenderPDFPagesToMetafileFailed, client_.get()));
}
#endif  // defined(WIN_PDF_METAFILE_FOR_PRINTING)

void ServiceUtilityProcessHost::OnGetPrinterCapsAndDefaultsSucceeded(
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& caps_and_defaults) {
  DCHECK(waiting_for_reply_);
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                            SERVICE_UTILITY_CAPS_SUCCEEDED,
                            SERVICE_UTILITY_EVENT_MAX);
  UMA_HISTOGRAM_TIMES("CloudPrint.ServiceUtilityCapsTime",
                      base::Time::Now() - start_time_);
  waiting_for_reply_ = false;
  client_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&Client::OnGetPrinterCapsAndDefaults, client_.get(), true,
                 printer_name, caps_and_defaults));
}

void ServiceUtilityProcessHost::OnGetPrinterSemanticCapsAndDefaultsSucceeded(
    const std::string& printer_name,
    const printing::PrinterSemanticCapsAndDefaults& caps_and_defaults) {
  DCHECK(waiting_for_reply_);
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                            SERVICE_UTILITY_SEMANTIC_CAPS_SUCCEEDED,
                            SERVICE_UTILITY_EVENT_MAX);
  UMA_HISTOGRAM_TIMES("CloudPrint.ServiceUtilitySemanticCapsTime",
                      base::Time::Now() - start_time_);
  waiting_for_reply_ = false;
  client_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&Client::OnGetPrinterSemanticCapsAndDefaults, client_.get(),
                 true, printer_name, caps_and_defaults));
}

void ServiceUtilityProcessHost::OnGetPrinterCapsAndDefaultsFailed(
    const std::string& printer_name) {
  DCHECK(waiting_for_reply_);
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                            SERVICE_UTILITY_CAPS_FAILED,
                            SERVICE_UTILITY_EVENT_MAX);
  UMA_HISTOGRAM_TIMES("CloudPrint.ServiceUtilityCapsFailTime",
                      base::Time::Now() - start_time_);
  waiting_for_reply_ = false;
  client_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&Client::OnGetPrinterCapsAndDefaults, client_.get(), false,
                 printer_name, printing::PrinterCapsAndDefaults()));
}

void ServiceUtilityProcessHost::OnGetPrinterSemanticCapsAndDefaultsFailed(
    const std::string& printer_name) {
  DCHECK(waiting_for_reply_);
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                            SERVICE_UTILITY_SEMANTIC_CAPS_FAILED,
                            SERVICE_UTILITY_EVENT_MAX);
  UMA_HISTOGRAM_TIMES("CloudPrint.ServiceUtilitySemanticCapsFailTime",
                      base::Time::Now() - start_time_);
  waiting_for_reply_ = false;
  client_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&Client::OnGetPrinterSemanticCapsAndDefaults,
                 client_.get(), false, printer_name,
                 printing::PrinterSemanticCapsAndDefaults()));
}

void ServiceUtilityProcessHost::Client::MetafileAvailable(
    const base::FilePath& metafile_path,
    int highest_rendered_page_number,
    double scale_factor) {
  // The metafile was created in a temp folder which needs to get deleted after
  // we have processed it.
  base::ScopedTempDir scratch_metafile_dir;
  if (!scratch_metafile_dir.Set(metafile_path.DirName()))
    LOG(WARNING) << "Unable to set scratch metafile directory";
#if defined(OS_WIN)
  // It's important that metafile is declared after scratch_metafile_dir so
  // that the metafile destructor closes the file before the base::ScopedTempDir
  // destructor tries to remove the directory.
  printing::Emf metafile;
  if (!metafile.InitFromFile(metafile_path)) {
    OnRenderPDFPagesToMetafileFailed();
  } else {
    OnRenderPDFPagesToMetafileSucceeded(metafile,
                                        highest_rendered_page_number,
                                        scale_factor);
  }
#endif  // defined(OS_WIN)
}

