// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_utility_process_host.h"

#include <stdint.h>

#include <queue>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/launch.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/win_util.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_utility_printing_messages.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mojo_channel_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_channel_pair.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "printing/emf_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_types.h"
#include "ui/base/ui_base_switches.h"

namespace {

using content::ChildProcessHost;

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
  SERVICE_UTILITY_FAILED_TO_START,
  SERVICE_UTILITY_EVENT_MAX,
};

void ReportUmaEvent(ServiceUtilityProcessHostEvent id) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceUtilityProcessHostEvent",
                            id,
                            SERVICE_UTILITY_EVENT_MAX);
}

// NOTE: changes to this class need to be reviewed by the security team.
class ServiceSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  ServiceSandboxedProcessLauncherDelegate() {}

  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    // Ignore result of SetAlternateDesktop. Service process may run as windows
    // service and it fails to create a window station.
    base::IgnoreResult(policy->SetAlternateDesktop(false));
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceSandboxedProcessLauncherDelegate);
};

}  // namespace

class ServiceUtilityProcessHost::PdfToEmfState {
 public:
  explicit PdfToEmfState(ServiceUtilityProcessHost* host)
      : host_(host), page_count_(0), current_page_(0), pages_in_progress_(0) {}
  ~PdfToEmfState() { Stop(); }

  bool Start(base::File pdf_file,
             const printing::PdfRenderSettings& conversion_settings) {
    if (!temp_dir_.CreateUniqueTempDir())
      return false;
    return host_->Send(new ChromeUtilityMsg_RenderPDFPagesToMetafiles(
        IPC::TakePlatformFileForTransit(std::move(pdf_file)),
        conversion_settings));
  }

  void GetMorePages() {
    const int kMaxNumberOfTempFilesPerDocument = 3;
    while (pages_in_progress_ < kMaxNumberOfTempFilesPerDocument &&
           current_page_ < page_count_) {
      ++pages_in_progress_;
      emf_files_.push(CreateTempFile());
      host_->Send(new ChromeUtilityMsg_RenderPDFPagesToMetafiles_GetPage(
          current_page_++,
          IPC::GetPlatformFileForTransit(
              emf_files_.back().GetPlatformFile(), false)));
    }
  }

  // Returns true if all pages processed and client should not expect more
  // results.
  bool OnPageProcessed() {
    --pages_in_progress_;
    GetMorePages();
    if (pages_in_progress_ || current_page_ < page_count_)
      return false;
    Stop();
    return true;
  }

  base::File TakeNextFile() {
    DCHECK(!emf_files_.empty());
    base::File file;
    if (!emf_files_.empty())
      file = std::move(emf_files_.front());
    emf_files_.pop();
    return file;
  }

  void set_page_count(int page_count) { page_count_ = page_count; }
  bool has_page_count() { return page_count_ > 0; }

 private:
  void Stop() {
    host_->Send(new ChromeUtilityMsg_RenderPDFPagesToMetafiles_Stop());
  }

  base::File CreateTempFile() {
    base::FilePath path;
    if (!base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &path))
      return base::File();
    return base::File(path,
                      base::File::FLAG_CREATE_ALWAYS |
                      base::File::FLAG_WRITE |
                      base::File::FLAG_READ |
                      base::File::FLAG_DELETE_ON_CLOSE |
                      base::File::FLAG_TEMPORARY);
  }

  base::ScopedTempDir temp_dir_;
  ServiceUtilityProcessHost* host_;
  std::queue<base::File> emf_files_;
  int page_count_;
  int current_page_;
  int pages_in_progress_;
};

ServiceUtilityProcessHost::ServiceUtilityProcessHost(
    Client* client,
    base::SingleThreadTaskRunner* client_task_runner)
    : client_(client),
      client_task_runner_(client_task_runner),
      waiting_for_reply_(false),
      weak_ptr_factory_(this) {
  child_process_host_.reset(ChildProcessHost::Create(this));
}

ServiceUtilityProcessHost::~ServiceUtilityProcessHost() {
  // We need to kill the child process when the host dies.
  process_.Terminate(content::RESULT_CODE_NORMAL_EXIT, false);
}

bool ServiceUtilityProcessHost::StartRenderPDFPagesToMetafile(
    const base::FilePath& pdf_path,
    const printing::PdfRenderSettings& render_settings) {
  ReportUmaEvent(SERVICE_UTILITY_METAFILE_REQUEST);
  base::File pdf_file(pdf_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_DELETE_ON_CLOSE);
  if (!pdf_file.IsValid() || !StartProcess(false))
    return false;

  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;

  pdf_to_emf_state_.reset(new PdfToEmfState(this));
  return pdf_to_emf_state_->Start(std::move(pdf_file), render_settings);
}

bool ServiceUtilityProcessHost::StartGetPrinterCapsAndDefaults(
    const std::string& printer_name) {
  ReportUmaEvent(SERVICE_UTILITY_CAPS_REQUEST);
  if (!StartProcess(true))
    return false;
  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;
  return Send(new ChromeUtilityMsg_GetPrinterCapsAndDefaults(printer_name));
}

bool ServiceUtilityProcessHost::StartGetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name) {
  ReportUmaEvent(SERVICE_UTILITY_SEMANTIC_CAPS_REQUEST);
  if (!StartProcess(true))
    return false;
  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;
  return Send(
      new ChromeUtilityMsg_GetPrinterSemanticCapsAndDefaults(printer_name));
}

bool ServiceUtilityProcessHost::StartProcess(bool no_sandbox) {
  std::string mojo_channel_token =
      child_process_host_->CreateChannelMojo(&process_connection_);
  if (mojo_channel_token.empty())
    return false;

  base::FilePath exe_path = GetUtilityProcessCmd();
  if (exe_path.empty()) {
    NOTREACHED() << "Unable to get utility process binary name.";
    return false;
  }

  base::CommandLine cmd_line(exe_path);
  cmd_line.AppendSwitchASCII(switches::kProcessType, switches::kUtilityProcess);
  cmd_line.AppendSwitchASCII(switches::kMojoChannelToken, mojo_channel_token);
  cmd_line.AppendSwitch(switches::kLang);
  cmd_line.AppendArg(switches::kPrefetchArgumentOther);

  if (Launch(&cmd_line, no_sandbox)) {
    ReportUmaEvent(SERVICE_UTILITY_STARTED);
    return true;
  }
  ReportUmaEvent(SERVICE_UTILITY_FAILED_TO_START);
  return false;
}

bool ServiceUtilityProcessHost::Launch(base::CommandLine* cmd_line,
                                       bool no_sandbox) {
  mojo::edk::ScopedPlatformHandle parent_handle;
  bool success = false;
  if (no_sandbox) {
    mojo::edk::NamedPlatformChannelPair named_pair;
    parent_handle = named_pair.PassServerHandle();
    named_pair.PrepareToPassClientHandleToChildProcess(cmd_line);

    cmd_line->AppendSwitch(switches::kNoSandbox);
    process_ = base::LaunchProcess(*cmd_line, base::LaunchOptions());
    success = process_.IsValid();
  } else {
    mojo::edk::PlatformChannelPair channel_pair;
    parent_handle = channel_pair.PassServerHandle();
    mojo::edk::ScopedPlatformHandle client_handle =
        channel_pair.PassClientHandle();
    base::HandlesToInheritVector handles;
    handles.push_back(client_handle.get().handle);
    cmd_line->AppendSwitchASCII(
        mojo::edk::PlatformChannelPair::kMojoPlatformChannelHandleSwitch,
        base::UintToString(base::win::HandleToUint32(handles[0])));

    ServiceSandboxedProcessLauncherDelegate delegate;
    base::Process process;
    sandbox::ResultCode result = content::StartSandboxedProcess(
        &delegate, cmd_line, handles, &process);
    if (result == sandbox::SBOX_ALL_OK) {
      process_ = std::move(process);
      success = true;
    }
  }

  if (success)
    process_connection_.Connect(process_.Handle(), std::move(parent_handle));

  return success;
}

bool ServiceUtilityProcessHost::Send(IPC::Message* msg) {
  if (child_process_host_)
    return child_process_host_->Send(msg);
  delete msg;
  return false;
}

base::FilePath ServiceUtilityProcessHost::GetUtilityProcessCmd() {
  return ChildProcessHost::GetChildPath(ChildProcessHost::CHILD_NORMAL);
}

void ServiceUtilityProcessHost::OnChildDisconnected() {
  if (waiting_for_reply_) {
    // If we are yet to receive a reply then notify the client that the
    // child died.
    client_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Client::OnChildDied, client_.get()));
    ReportUmaEvent(SERVICE_UTILITY_DISCONNECTED);
  }
  delete this;
}

bool ServiceUtilityProcessHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceUtilityProcessHost, message)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_RenderPDFPagesToMetafiles_PageCount,
        OnRenderPDFPagesToMetafilesPageCount)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_RenderPDFPagesToMetafiles_PageDone,
                        OnRenderPDFPagesToMetafilesPageDone)
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

const base::Process& ServiceUtilityProcessHost::GetProcess() const {
  return process_;
}

void ServiceUtilityProcessHost::OnMetafileSpooled(bool success) {
  if (!success || pdf_to_emf_state_->OnPageProcessed())
    OnPDFToEmfFinished(success);
}

void ServiceUtilityProcessHost::OnRenderPDFPagesToMetafilesPageCount(
    int page_count) {
  DCHECK(waiting_for_reply_);
  if (!pdf_to_emf_state_ || page_count <= 0 ||
      pdf_to_emf_state_->has_page_count()) {
    return OnPDFToEmfFinished(false);
  }
  pdf_to_emf_state_->set_page_count(page_count);
  pdf_to_emf_state_->GetMorePages();
}

void ServiceUtilityProcessHost::OnRenderPDFPagesToMetafilesPageDone(
    bool success,
    float scale_factor) {
  DCHECK(waiting_for_reply_);
  if (!pdf_to_emf_state_ || !success)
    return OnPDFToEmfFinished(false);
  base::File emf_file = pdf_to_emf_state_->TakeNextFile();
  base::PostTaskAndReplyWithResult(
      client_task_runner_.get(), FROM_HERE,
      base::Bind(&Client::MetafileAvailable, client_.get(), scale_factor,
                 base::Passed(&emf_file)),
      base::Bind(&ServiceUtilityProcessHost::OnMetafileSpooled,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ServiceUtilityProcessHost::OnPDFToEmfFinished(bool success) {
  if (!waiting_for_reply_)
    return;
  waiting_for_reply_ = false;
  if (success) {
    ReportUmaEvent(SERVICE_UTILITY_METAFILE_SUCCEEDED);
  } else {
    ReportUmaEvent(SERVICE_UTILITY_METAFILE_FAILED);
  }
  client_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Client::OnRenderPDFPagesToMetafileDone,
                            client_.get(), success));
  pdf_to_emf_state_.reset();
}

void ServiceUtilityProcessHost::OnGetPrinterCapsAndDefaultsSucceeded(
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& caps_and_defaults) {
  DCHECK(waiting_for_reply_);
  ReportUmaEvent(SERVICE_UTILITY_CAPS_SUCCEEDED);
  waiting_for_reply_ = false;
  client_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Client::OnGetPrinterCapsAndDefaults, client_.get(),
                            true, printer_name, caps_and_defaults));
}

void ServiceUtilityProcessHost::OnGetPrinterSemanticCapsAndDefaultsSucceeded(
    const std::string& printer_name,
    const printing::PrinterSemanticCapsAndDefaults& caps_and_defaults) {
  DCHECK(waiting_for_reply_);
  ReportUmaEvent(SERVICE_UTILITY_SEMANTIC_CAPS_SUCCEEDED);
  waiting_for_reply_ = false;
  client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Client::OnGetPrinterSemanticCapsAndDefaults, client_.get(),
                 true, printer_name, caps_and_defaults));
}

void ServiceUtilityProcessHost::OnGetPrinterCapsAndDefaultsFailed(
    const std::string& printer_name) {
  DCHECK(waiting_for_reply_);
  ReportUmaEvent(SERVICE_UTILITY_CAPS_FAILED);
  waiting_for_reply_ = false;
  client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Client::OnGetPrinterCapsAndDefaults, client_.get(), false,
                 printer_name, printing::PrinterCapsAndDefaults()));
}

void ServiceUtilityProcessHost::OnGetPrinterSemanticCapsAndDefaultsFailed(
    const std::string& printer_name) {
  DCHECK(waiting_for_reply_);
  ReportUmaEvent(SERVICE_UTILITY_SEMANTIC_CAPS_FAILED);
  waiting_for_reply_ = false;
  client_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Client::OnGetPrinterSemanticCapsAndDefaults,
                            client_.get(), false, printer_name,
                            printing::PrinterSemanticCapsAndDefaults()));
}

bool ServiceUtilityProcessHost::Client::MetafileAvailable(float scale_factor,
                                                          base::File file) {
  file.Seek(base::File::FROM_BEGIN, 0);
  int64_t size = file.GetLength();
  if (size <= 0) {
    OnRenderPDFPagesToMetafileDone(false);
    return false;
  }
  std::vector<char> data(size);
  if (file.ReadAtCurrentPos(data.data(), data.size()) != size) {
    OnRenderPDFPagesToMetafileDone(false);
    return false;
  }
  printing::Emf emf;
  if (!emf.InitFromData(data.data(), data.size())) {
    OnRenderPDFPagesToMetafileDone(false);
    return false;
  }
  OnRenderPDFPagesToMetafilePageDone(scale_factor, emf);
  return true;
}
