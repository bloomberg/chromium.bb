// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "chrome/common/safe_browsing/zip_analyzer_results.h"
#include "chrome/utility/chrome_content_utility_ipc_whitelist.h"
#include "chrome/utility/utility_message_handler.h"
#include "components/safe_json/utility/safe_json_parser_mojo_impl.h"
#include "content/public/child/image_decoder_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_info.h"
#include "content/public/utility/utility_thread.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"
#include "extensions/features/features.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "printing/features/features.h"
#include "services/image_decoder/image_decoder_service.h"
#include "services/image_decoder/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "third_party/zlib/google/zip.h"
#include "ui/gfx/geometry/size.h"

#if !defined(OS_ANDROID)
#include "chrome/common/resource_usage_reporter.mojom.h"
#include "chrome/utility/profile_import_handler.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/proxy/mojo_proxy_resolver_factory_impl.h"
#include "net/proxy/proxy_resolver_v8.h"
#endif

#if defined(OS_WIN)
#include "chrome/utility/ipc_shell_handler_win.h"
#include "chrome/utility/shell_handler_impl_win.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/utility/extensions/extensions_handler.h"
#include "chrome/utility/image_writer/image_writer_handler.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_BASIC_PRINTING) && defined(OS_WIN))
#include "chrome/utility/printing_handler.h"
#endif

#if defined(OS_MACOSX) && defined(FULL_SAFE_BROWSING)
#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"
#endif

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ReleaseProcessIfNeeded() {
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

#if !defined(OS_ANDROID)
void CreateProxyResolverFactory(
    net::interfaces::ProxyResolverFactoryRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<net::MojoProxyResolverFactoryImpl>(),
                          std::move(request));
}

class ResourceUsageReporterImpl : public chrome::mojom::ResourceUsageReporter {
 public:
  ResourceUsageReporterImpl() {}
  ~ResourceUsageReporterImpl() override {}

 private:
  void GetUsageData(const GetUsageDataCallback& callback) override {
    chrome::mojom::ResourceUsageDataPtr data =
        chrome::mojom::ResourceUsageData::New();
    size_t total_heap_size = net::ProxyResolverV8::GetTotalHeapSize();
    if (total_heap_size) {
      data->reports_v8_stats = true;
      data->v8_bytes_allocated = total_heap_size;
      data->v8_bytes_used = net::ProxyResolverV8::GetUsedHeapSize();
    }
    callback.Run(std::move(data));
  }
};

void CreateResourceUsageReporter(
    mojo::InterfaceRequest<chrome::mojom::ResourceUsageReporter> request) {
  mojo::MakeStrongBinding(base::MakeUnique<ResourceUsageReporterImpl>(),
                          std::move(request));
}
#endif  // !defined(OS_ANDROID)

std::unique_ptr<service_manager::Service> CreateImageDecoderService() {
  content::UtilityThread::Get()->EnsureBlinkInitialized();
  return image_decoder::ImageDecoderService::Create();
}

}  // namespace

ChromeContentUtilityClient::ChromeContentUtilityClient()
    : filter_messages_(false) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  handlers_.push_back(new extensions::ExtensionsHandler(this));
  handlers_.push_back(new image_writer::ImageWriterHandler());
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_BASIC_PRINTING) && defined(OS_WIN))
  handlers_.push_back(new printing::PrintingHandler());
#endif

#if defined(OS_WIN)
  handlers_.push_back(new IPCShellHandler());
#endif
}

ChromeContentUtilityClient::~ChromeContentUtilityClient() {
}

void ChromeContentUtilityClient::UtilityThreadStarted() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::UtilityHandler::UtilityThreadStarted();
#endif

  if (kMessageWhitelistSize > 0) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUtilityProcessRunningElevated)) {
      message_id_whitelist_.insert(kMessageWhitelist,
                                   kMessageWhitelist + kMessageWhitelistSize);
      filter_messages_ = true;
    }
  }
}

bool ChromeContentUtilityClient::OnMessageReceived(
    const IPC::Message& message) {
  if (filter_messages_ &&
      !base::ContainsKey(message_id_whitelist_, message.type())) {
    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeContentUtilityClient, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_PatchFileBsdiff,
                        OnPatchFileBsdiff)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_PatchFileCourgette,
                        OnPatchFileCourgette)
#if defined(FULL_SAFE_BROWSING)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_AnalyzeZipFileForDownloadProtection,
                        OnAnalyzeZipFileForDownloadProtection)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_AnalyzeDmgFileForDownloadProtection,
                        OnAnalyzeDmgFileForDownloadProtection)
#endif  // defined(OS_MACOSX)
#endif  // defined(FULL_SAFE_BROWSING)
#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_CreateZipFile, OnCreateZipFile)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (handled)
    return true;

  for (auto* handler : handlers_) {
    // At least one of the utility process handlers adds a new handler to
    // |handlers_| when it handles a message. This causes any iterator over
    // |handlers_| to become invalid. Therefore, it is necessary to break the
    // loop at this point instead of evaluating it as a loop condition (if the
    // for loop was using iterators explicitly, as originally done).
    if (handler->OnMessageReceived(message))
      return true;
  }

  return false;
}

void ChromeContentUtilityClient::ExposeInterfacesToBrowser(
    service_manager::InterfaceRegistry* registry) {
  // When the utility process is running with elevated privileges, we need to
  // filter messages so that only a whitelist of IPCs can run. In Mojo, there's
  // no way of filtering individual messages. Instead, we can avoid adding
  // non-whitelisted Mojo services to the service_manager::InterfaceRegistry.
  // TODO(amistry): Use a whitelist once the whistlisted IPCs have been
  // converted to Mojo.
  if (filter_messages_)
    return;

#if !defined(OS_ANDROID)
  registry->AddInterface<net::interfaces::ProxyResolverFactory>(
      base::Bind(CreateProxyResolverFactory));
  registry->AddInterface(base::Bind(CreateResourceUsageReporter));
  registry->AddInterface(base::Bind(ProfileImportHandler::Create));
#endif
  registry->AddInterface(
      base::Bind(&safe_json::SafeJsonParserMojoImpl::Create));
#if defined(OS_WIN)
  registry->AddInterface(base::Bind(&ShellHandlerImpl::Create));
#endif
}

void ChromeContentUtilityClient::RegisterServices(StaticServiceMap* services) {
  content::ServiceInfo image_decoder_info;
  image_decoder_info.factory = base::Bind(&CreateImageDecoderService);
  services->insert(
      std::make_pair(image_decoder::mojom::kServiceName, image_decoder_info));
}

void ChromeContentUtilityClient::AddHandler(
    std::unique_ptr<UtilityMessageHandler> handler) {
  handlers_.push_back(std::move(handler));
}

// static
void ChromeContentUtilityClient::PreSandboxStartup() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionsHandler::PreSandboxStartup();
#endif
}

#if defined(OS_CHROMEOS)
void ChromeContentUtilityClient::OnCreateZipFile(
    const base::FilePath& src_dir,
    const std::vector<base::FilePath>& src_relative_paths,
    const base::FileDescriptor& dest_fd) {
  // dest_fd should be closed in the function. See ipc/ipc_message_util.h for
  // details.
  base::ScopedFD fd_closer(dest_fd.fd);
  bool succeeded = true;

  // Check sanity of source relative paths. Reject if path is absolute or
  // contains any attempt to reference a parent directory ("../" tricks).
  for (std::vector<base::FilePath>::const_iterator iter =
           src_relative_paths.begin(); iter != src_relative_paths.end();
       ++iter) {
    if (iter->IsAbsolute() || iter->ReferencesParent()) {
      succeeded = false;
      break;
    }
  }

  if (succeeded)
    succeeded = zip::ZipFiles(src_dir, src_relative_paths, dest_fd.fd);

  if (succeeded)
    Send(new ChromeUtilityHostMsg_CreateZipFile_Succeeded());
  else
    Send(new ChromeUtilityHostMsg_CreateZipFile_Failed());
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_CHROMEOS)

void ChromeContentUtilityClient::OnPatchFileBsdiff(
    const base::FilePath& input_file,
    const base::FilePath& patch_file,
    const base::FilePath& output_file) {
  if (input_file.empty() || patch_file.empty() || output_file.empty()) {
    Send(new ChromeUtilityHostMsg_PatchFile_Finished(-1));
  } else {
    const int patch_status = bsdiff::ApplyBinaryPatch(input_file,
                                                      patch_file,
                                                      output_file);
    Send(new ChromeUtilityHostMsg_PatchFile_Finished(patch_status));
  }
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnPatchFileCourgette(
    const base::FilePath& input_file,
    const base::FilePath& patch_file,
    const base::FilePath& output_file) {
  if (input_file.empty() || patch_file.empty() || output_file.empty()) {
    Send(new ChromeUtilityHostMsg_PatchFile_Finished(-1));
  } else {
    const int patch_status = courgette::ApplyEnsemblePatch(
        input_file.value().c_str(),
        patch_file.value().c_str(),
        output_file.value().c_str());
    Send(new ChromeUtilityHostMsg_PatchFile_Finished(patch_status));
  }
  ReleaseProcessIfNeeded();
}

#if defined(FULL_SAFE_BROWSING)
void ChromeContentUtilityClient::OnAnalyzeZipFileForDownloadProtection(
    const IPC::PlatformFileForTransit& zip_file,
    const IPC::PlatformFileForTransit& temp_file) {
  safe_browsing::zip_analyzer::Results results;
  safe_browsing::zip_analyzer::AnalyzeZipFile(
      IPC::PlatformFileForTransitToFile(zip_file),
      IPC::PlatformFileForTransitToFile(temp_file), &results);
  Send(new ChromeUtilityHostMsg_AnalyzeZipFileForDownloadProtection_Finished(
      results));
  ReleaseProcessIfNeeded();
}

#if defined(OS_MACOSX)
void ChromeContentUtilityClient::OnAnalyzeDmgFileForDownloadProtection(
    const IPC::PlatformFileForTransit& dmg_file) {
  safe_browsing::zip_analyzer::Results results;
  safe_browsing::dmg::AnalyzeDMGFile(
      IPC::PlatformFileForTransitToFile(dmg_file), &results);
  Send(new ChromeUtilityHostMsg_AnalyzeDmgFileForDownloadProtection_Finished(
      results));
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_MACOSX)

#endif  // defined(FULL_SAFE_BROWSING)
