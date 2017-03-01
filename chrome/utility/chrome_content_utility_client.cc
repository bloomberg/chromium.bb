// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/file_patcher.mojom.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "chrome/common/safe_browsing/zip_analyzer_results.h"
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

#if defined(OS_CHROMEOS)
#include "chrome/common/zip_file_creator.mojom.h"
#endif

#if defined(OS_WIN)
#include "chrome/utility/ipc_shell_handler_win.h"
#include "chrome/utility/shell_handler_impl_win.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/utility/extensions/extensions_handler.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_BASIC_PRINTING) && defined(OS_WIN))
#include "chrome/utility/printing_handler.h"
#endif

#if defined(OS_MACOSX) && defined(FULL_SAFE_BROWSING)
#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"
#endif

namespace {

class FilePatcherImpl : public chrome::mojom::FilePatcher {
 public:
  FilePatcherImpl() = default;
  ~FilePatcherImpl() override = default;

  static void Create(chrome::mojom::FilePatcherRequest request) {
    mojo::MakeStrongBinding(base::MakeUnique<FilePatcherImpl>(),
                            std::move(request));
  }

 private:
  // chrome::mojom::FilePatcher:
  void PatchFileBsdiff(base::File input_file,
                       base::File patch_file,
                       base::File output_file,
                       const PatchFileBsdiffCallback& callback) override {
    const int patch_result_status = bsdiff::ApplyBinaryPatch(
        std::move(input_file), std::move(patch_file), std::move(output_file));
    callback.Run(patch_result_status);
  }

  void PatchFileCourgette(base::File input_file,
                          base::File patch_file,
                          base::File output_file,
                          const PatchFileCourgetteCallback& callback) override {
    const int patch_result_status = courgette::ApplyEnsemblePatch(
        std::move(input_file), std::move(patch_file), std::move(output_file));
    callback.Run(patch_result_status);
  }

  DISALLOW_COPY_AND_ASSIGN(FilePatcherImpl);
};

#if defined(OS_CHROMEOS)
class ZipFileCreatorImpl : public chrome::mojom::ZipFileCreator {
 public:
  ZipFileCreatorImpl() = default;
  ~ZipFileCreatorImpl() override = default;

  static void Create(chrome::mojom::ZipFileCreatorRequest request) {
    mojo::MakeStrongBinding(base::MakeUnique<ZipFileCreatorImpl>(),
                            std::move(request));
  }

 private:
  // chrome::mojom::ZipFileCreator:
  void CreateZipFile(const base::FilePath& source_dir,
                     const std::vector<base::FilePath>& source_relative_paths,
                     base::File zip_file,
                     const CreateZipFileCallback& callback) override {
    DCHECK(zip_file.IsValid());

    for (const auto& path : source_relative_paths) {
      if (path.IsAbsolute() || path.ReferencesParent()) {
        callback.Run(false);
        return;
      }
    }

    callback.Run(zip::ZipFiles(source_dir, source_relative_paths,
                               zip_file.GetPlatformFile()));
  }

  DISALLOW_COPY_AND_ASSIGN(ZipFileCreatorImpl);
};
#endif  // defined(OS_CHROMEOS)

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

  DISALLOW_COPY_AND_ASSIGN(ResourceUsageReporterImpl);
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
    : utility_process_running_elevated_(false) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  handlers_.push_back(new extensions::ExtensionsHandler());
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_BASIC_PRINTING) && defined(OS_WIN))
  handlers_.push_back(new printing::PrintingHandler());
#endif

#if defined(OS_WIN)
  handlers_.push_back(new IPCShellHandler());
#endif
}

ChromeContentUtilityClient::~ChromeContentUtilityClient() = default;

void ChromeContentUtilityClient::UtilityThreadStarted() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::UtilityHandler::UtilityThreadStarted();
#endif

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUtilityProcessRunningElevated))
    utility_process_running_elevated_ = true;
}

bool ChromeContentUtilityClient::OnMessageReceived(
    const IPC::Message& message) {
  if (utility_process_running_elevated_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeContentUtilityClient, message)
#if defined(FULL_SAFE_BROWSING)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_AnalyzeZipFileForDownloadProtection,
                        OnAnalyzeZipFileForDownloadProtection)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_AnalyzeDmgFileForDownloadProtection,
                        OnAnalyzeDmgFileForDownloadProtection)
#endif  // defined(OS_MACOSX)
#endif  // defined(FULL_SAFE_BROWSING)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (handled)
    return true;

  for (auto* handler : handlers_) {
    if (handler->OnMessageReceived(message))
      return true;
  }

  return false;
}

void ChromeContentUtilityClient::ExposeInterfacesToBrowser(
    service_manager::InterfaceRegistry* registry) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionsHandler::ExposeInterfacesToBrowser(
      registry, utility_process_running_elevated_);
#endif
  // If our process runs with elevated privileges, only add elevated Mojo
  // services to the interface registry.
  if (utility_process_running_elevated_)
    return;

  registry->AddInterface(base::Bind(&FilePatcherImpl::Create));
#if !defined(OS_ANDROID)
  registry->AddInterface<net::interfaces::ProxyResolverFactory>(
      base::Bind(CreateProxyResolverFactory));
  registry->AddInterface(base::Bind(CreateResourceUsageReporter));
  registry->AddInterface(base::Bind(&ProfileImportHandler::Create));
#endif
  registry->AddInterface(
      base::Bind(&safe_json::SafeJsonParserMojoImpl::Create));
#if defined(OS_WIN)
  registry->AddInterface(base::Bind(&ShellHandlerImpl::Create));
#endif
#if defined(OS_CHROMEOS)
  registry->AddInterface(base::Bind(&ZipFileCreatorImpl::Create));
#endif
}

void ChromeContentUtilityClient::RegisterServices(StaticServiceMap* services) {
  content::ServiceInfo image_decoder_info;
  image_decoder_info.factory = base::Bind(&CreateImageDecoderService);
  services->insert(
      std::make_pair(image_decoder::mojom::kServiceName, image_decoder_info));
}

// static
void ChromeContentUtilityClient::PreSandboxStartup() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionsHandler::PreSandboxStartup();
#endif
}

#if defined(FULL_SAFE_BROWSING)
void ChromeContentUtilityClient::OnAnalyzeZipFileForDownloadProtection(
    const IPC::PlatformFileForTransit& zip_file,
    const IPC::PlatformFileForTransit& temp_file) {
  safe_browsing::zip_analyzer::Results results;
  safe_browsing::zip_analyzer::AnalyzeZipFile(
      IPC::PlatformFileForTransitToFile(zip_file),
      IPC::PlatformFileForTransitToFile(temp_file), &results);
  content::UtilityThread::Get()->Send(
      new ChromeUtilityHostMsg_AnalyzeZipFileForDownloadProtection_Finished(
          results));
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

#if defined(OS_MACOSX)
void ChromeContentUtilityClient::OnAnalyzeDmgFileForDownloadProtection(
    const IPC::PlatformFileForTransit& dmg_file) {
  safe_browsing::zip_analyzer::Results results;
  safe_browsing::dmg::AnalyzeDMGFile(
      IPC::PlatformFileForTransitToFile(dmg_file), &results);
  content::UtilityThread::Get()->Send(
      new ChromeUtilityHostMsg_AnalyzeDmgFileForDownloadProtection_Finished(
          results));
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}
#endif  // defined(OS_MACOSX)

#endif  // defined(FULL_SAFE_BROWSING)
