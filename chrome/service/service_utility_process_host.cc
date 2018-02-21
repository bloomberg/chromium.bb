// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_utility_process_host.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
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
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_utility_printing_messages.h"
#include "chrome/services/printing/public/mojom/pdf_to_emf_converter.mojom.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mojo_channel_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_channel_pair.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "printing/emf_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_types.h"
#include "services/service_manager/sandbox/sandbox_type.h"
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

  service_manager::SandboxType GetSandboxType() override {
    return service_manager::SANDBOX_TYPE_UTILITY;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceSandboxedProcessLauncherDelegate);
};

// This implementation does not do any font pre-caching.
// TODO(thestig): Can this be deleted and the PdfToEmfConverterClient be made
// optional?
class ServicePdfToEmfConverterClientImpl
    : public printing::mojom::PdfToEmfConverterClient {
 public:
  explicit ServicePdfToEmfConverterClientImpl(
      printing::mojom::PdfToEmfConverterClientRequest request)
      : binding_(this, std::move(request)) {}

 private:
  // mojom::PdfToEmfConverterClient:
  void PreCacheFontCharacters(
      const std::vector<uint8_t>& logfont_data,
      const base::string16& characters,
      PreCacheFontCharactersCallback callback) override {
    std::move(callback).Run();
  }

  mojo::Binding<printing::mojom::PdfToEmfConverterClient> binding_;
};

}  // namespace

class ServiceUtilityProcessHost::PdfToEmfState {
 public:
  explicit PdfToEmfState(base::WeakPtr<ServiceUtilityProcessHost> host)
      : weak_host_(host) {}

  ~PdfToEmfState() { Stop(); }

  bool Start(base::File pdf_file,
             const printing::PdfRenderSettings& conversion_settings) {
    if (!temp_dir_.CreateUniqueTempDir())
      return false;

    weak_host_->BindInterface(
        printing::mojom::PdfToEmfConverterFactory::Name_,
        mojo::MakeRequest(&pdf_to_emf_converter_factory_).PassMessagePipe());

    pdf_to_emf_converter_factory_.set_connection_error_handler(base::BindOnce(
        &PdfToEmfState::OnFailed, weak_host_,
        std::string("Connection to PdfToEmfConverterFactory error.")));

    printing::mojom::PdfToEmfConverterClientPtr pdf_to_emf_converter_client_ptr;
    pdf_to_emf_converter_client_impl_ =
        std::make_unique<ServicePdfToEmfConverterClientImpl>(
            mojo::MakeRequest(&pdf_to_emf_converter_client_ptr));

    pdf_to_emf_converter_factory_->CreateConverter(
        mojo::WrapPlatformFile(pdf_file.TakePlatformFile()),
        conversion_settings, std::move(pdf_to_emf_converter_client_ptr),
        base::BindOnce(
            &ServiceUtilityProcessHost::OnRenderPDFPagesToMetafilesPageCount,
            weak_host_));
    return true;
  }

  void GotPageCount(printing::mojom::PdfToEmfConverterPtr converter,
                    uint32_t page_count) {
    DCHECK(!pdf_to_emf_converter_.is_bound());
    pdf_to_emf_converter_ = std::move(converter);
    pdf_to_emf_converter_.set_connection_error_handler(
        base::BindOnce(&PdfToEmfState::OnFailed, weak_host_,
                       std::string("Connection to PdfToEmfConverter error.")));
    page_count_ = page_count;
  }

  void GetMorePages() {
    const int kMaxNumberOfTempFilesPerDocument = 3;
    while (pages_in_progress_ < kMaxNumberOfTempFilesPerDocument &&
           current_page_ < page_count_) {
      ++pages_in_progress_;
      emf_files_.push(CreateTempFile());

      // We need to dup the file as mojo::WrapPlatformFile takes ownership of
      // the passed file.
      base::File temp_file_copy = emf_files_.back().Duplicate();
      pdf_to_emf_converter_->ConvertPage(
          current_page_++,
          mojo::WrapPlatformFile(temp_file_copy.TakePlatformFile()),
          base::BindOnce(
              &ServiceUtilityProcessHost::OnRenderPDFPagesToMetafilesPageDone,
              weak_host_));
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

  bool has_page_count() const { return page_count_ > 0; }

 private:
  static void OnFailed(const base::WeakPtr<ServiceUtilityProcessHost>& host,
                       const std::string& error_message) {
    LOG(ERROR) << "Failed to convert PDF: " << error_message;
    host->OnChildDisconnected();
  }

  void Stop() {
    // Disconnect interface ptrs so that the printing service process stop.
    pdf_to_emf_converter_factory_.reset();
    pdf_to_emf_converter_.reset();
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
  base::WeakPtr<ServiceUtilityProcessHost> weak_host_;
  base::queue<base::File> emf_files_;
  int page_count_ = 0;
  int current_page_ = 0;
  int pages_in_progress_ = 0;

  std::unique_ptr<ServicePdfToEmfConverterClientImpl>
      pdf_to_emf_converter_client_impl_;

  printing::mojom::PdfToEmfConverterPtr pdf_to_emf_converter_;

  printing::mojom::PdfToEmfConverterFactoryPtr pdf_to_emf_converter_factory_;
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
  if (!pdf_file.IsValid() || !StartProcess(/*sandbox=*/true))
    return false;

  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;

  pdf_to_emf_state_ =
      std::make_unique<PdfToEmfState>(weak_ptr_factory_.GetWeakPtr());
  return pdf_to_emf_state_->Start(std::move(pdf_file), render_settings);
}

bool ServiceUtilityProcessHost::StartGetPrinterCapsAndDefaults(
    const std::string& printer_name) {
  ReportUmaEvent(SERVICE_UTILITY_CAPS_REQUEST);
  if (!StartProcess(/*sandbox=*/false))
    return false;
  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;
  return Send(new ChromeUtilityMsg_GetPrinterCapsAndDefaults(printer_name));
}

bool ServiceUtilityProcessHost::StartGetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name) {
  ReportUmaEvent(SERVICE_UTILITY_SEMANTIC_CAPS_REQUEST);
  if (!StartProcess(/*sandbox=*/false))
    return false;
  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;
  return Send(
      new ChromeUtilityMsg_GetPrinterSemanticCapsAndDefaults(printer_name));
}

bool ServiceUtilityProcessHost::StartProcess(bool sandbox) {
  base::FilePath exe_path = GetUtilityProcessCmd();
  if (exe_path.empty()) {
    NOTREACHED() << "Unable to get utility process binary name.";
    return false;
  }

  std::string mojo_bootstrap_token = mojo::edk::GenerateRandomToken();
  utility_process_connection_.Bind(service_manager::mojom::ServicePtrInfo(
      broker_client_invitation_.AttachMessagePipe(mojo_bootstrap_token), 0u));
  child_process_host_->CreateChannelMojo();

  base::CommandLine cmd_line(exe_path);
  cmd_line.AppendSwitchASCII(switches::kProcessType, switches::kUtilityProcess);
  cmd_line.AppendSwitchASCII(switches::kServiceRequestChannelToken,
                             mojo_bootstrap_token);
  cmd_line.AppendSwitch(switches::kLang);
  cmd_line.AppendArg(switches::kPrefetchArgumentOther);

  if (Launch(&cmd_line, sandbox)) {
    ReportUmaEvent(SERVICE_UTILITY_STARTED);
    return true;
  }
  ReportUmaEvent(SERVICE_UTILITY_FAILED_TO_START);
  return false;
}

bool ServiceUtilityProcessHost::Launch(base::CommandLine* cmd_line,
                                       bool sandbox) {
  const base::CommandLine& service_command_line =
      *base::CommandLine::ForCurrentProcess();
  static const char* const kForwardSwitches[] = {
      switches::kDisableLogging,
      switches::kEnableLogging,
      switches::kIPCConnectionTimeout,
      switches::kLoggingLevel,
      switches::kUtilityStartupDialog,
      switches::kV,
      switches::kVModule,
  };
  cmd_line->CopySwitchesFrom(service_command_line, kForwardSwitches,
                             arraysize(kForwardSwitches));

  mojo::edk::ScopedPlatformHandle parent_handle;
  bool success = false;
  if (sandbox) {
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
  } else {
    mojo::edk::NamedPlatformChannelPair named_pair;
    parent_handle = named_pair.PassServerHandle();
    named_pair.PrepareToPassClientHandleToChildProcess(cmd_line);

    cmd_line->AppendSwitch(switches::kNoSandbox);
    process_ = base::LaunchProcess(*cmd_line, base::LaunchOptions());
    success = process_.IsValid();
  }

  if (success) {
    broker_client_invitation_.Send(
        process_.Handle(),
        mojo::edk::ConnectionParams(mojo::edk::TransportProtocol::kLegacy,
                                    std::move(parent_handle)));
  }

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

void ServiceUtilityProcessHost::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  service_manager::BindSourceInfo source_info;
  // ChildThreadImpl expects a connection from the browser process for
  // establishing its legacy IPC channel.
  source_info.identity =
      service_manager::Identity{content::mojom::kBrowserServiceName};
  utility_process_connection_->OnBindInterface(source_info, interface_name,
                                               std::move(interface_pipe),
                                               base::Bind(&base::DoNothing));
}

void ServiceUtilityProcessHost::OnMetafileSpooled(bool success) {
  if (!success || pdf_to_emf_state_->OnPageProcessed())
    OnPDFToEmfFinished(success);
}

void ServiceUtilityProcessHost::OnRenderPDFPagesToMetafilesPageCount(
    printing::mojom::PdfToEmfConverterPtr converter,
    uint32_t page_count) {
  DCHECK(waiting_for_reply_);
  if (page_count == 0 || pdf_to_emf_state_->has_page_count())
    return OnPDFToEmfFinished(false);

  pdf_to_emf_state_->GotPageCount(std::move(converter), page_count);
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
  ReportUmaEvent(success ? SERVICE_UTILITY_METAFILE_SUCCEEDED
                         : SERVICE_UTILITY_METAFILE_FAILED);
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
