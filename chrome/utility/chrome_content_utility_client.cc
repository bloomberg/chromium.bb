// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/features.h"
#include "chrome/common/file_patcher.mojom.h"
#include "chrome/common/profiling/constants.mojom.h"
#include "chrome/profiling/profiling_service.h"
#include "chrome/utility/printing/pdf_to_pwg_raster_converter_impl.h"
#include "chrome/utility/utility_message_handler.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "components/safe_json/utility/safe_json_parser_mojo_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/utility/utility_thread.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"
#include "extensions/features/features.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "printing/features/features.h"
#include "services/service_manager/embedder/embedded_service_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/zlib/google/zip.h"

#if !defined(OS_ANDROID)
#include "chrome/common/resource_usage_reporter.mojom.h"
#include "chrome/utility/importer/profile_import_impl.h"
#include "chrome/utility/importer/profile_import_service.h"
#include "chrome/utility/media_router/dial_device_description_parser_impl.h"
#include "net/proxy/mojo_proxy_resolver_factory_impl.h"  // nogncheck
#include "net/proxy/proxy_resolver_v8.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/common/zip_file_creator.mojom.h"
#endif

#if defined(OS_WIN)
#include "chrome/utility/shell_handler_impl_win.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/utility/extensions/extensions_handler.h"
#endif

#if BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)
#include "chrome/utility/mash_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/common/printing/pdf_to_pwg_raster_converter.mojom.h"
#include "chrome/utility/printing/pdf_to_pwg_raster_converter_service.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_BASIC_PRINTING) && defined(OS_WIN))
#include "chrome/utility/printing_handler.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/common/chrome_content_client.h"
#include "components/printing/service/public/cpp/pdf_compositor_service_factory.h"
#include "components/printing/service/public/interfaces/pdf_compositor.mojom.h"  // nogncheck
#endif

#if defined(FULL_SAFE_BROWSING)
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/safe_archive_analyzer.mojom.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#if defined(OS_MACOSX)
#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"
#endif
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
    DCHECK(input_file.IsValid());
    DCHECK(patch_file.IsValid());
    DCHECK(output_file.IsValid());

    const int patch_result_status = bsdiff::ApplyBinaryPatch(
        std::move(input_file), std::move(patch_file), std::move(output_file));
    callback.Run(patch_result_status);
  }

  void PatchFileCourgette(base::File input_file,
                          base::File patch_file,
                          base::File output_file,
                          const PatchFileCourgetteCallback& callback) override {
    DCHECK(input_file.IsValid());
    DCHECK(patch_file.IsValid());
    DCHECK(output_file.IsValid());

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

#if defined(FULL_SAFE_BROWSING)
class SafeArchiveAnalyzerImpl : public chrome::mojom::SafeArchiveAnalyzer {
 public:
  SafeArchiveAnalyzerImpl() = default;
  ~SafeArchiveAnalyzerImpl() override = default;

  static void Create(chrome::mojom::SafeArchiveAnalyzerRequest request) {
    mojo::MakeStrongBinding(base::MakeUnique<SafeArchiveAnalyzerImpl>(),
                            std::move(request));
  }

 private:
  // chrome::mojom::SafeArchiveAnalyzer:
  void AnalyzeZipFile(base::File zip_file,
                      base::File temporary_file,
                      const AnalyzeZipFileCallback& callback) override {
    DCHECK(temporary_file.IsValid());
    DCHECK(zip_file.IsValid());

    safe_browsing::ArchiveAnalyzerResults results;
    safe_browsing::zip_analyzer::AnalyzeZipFile(
        std::move(zip_file), std::move(temporary_file), &results);
    callback.Run(results);
  }

  void AnalyzeDmgFile(base::File dmg_file,
                      const AnalyzeDmgFileCallback& callback) override {
#if defined(OS_MACOSX)
    DCHECK(dmg_file.IsValid());
    safe_browsing::ArchiveAnalyzerResults results;
    safe_browsing::dmg::AnalyzeDMGFile(std::move(dmg_file), &results);
    callback.Run(results);
#else
    NOTREACHED();
#endif
  }

  DISALLOW_COPY_AND_ASSIGN(SafeArchiveAnalyzerImpl);
};
#endif  // defined(FULL_SAFE_BROWSING)

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
    chrome::mojom::ResourceUsageReporterRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<ResourceUsageReporterImpl>(),
                          std::move(request));
}
#endif  // !defined(OS_ANDROID)

base::LazyInstance<ChromeContentUtilityClient::NetworkBinderCreationCallback>::
    Leaky g_network_binder_creation_callback = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromeContentUtilityClient::ChromeContentUtilityClient()
    : utility_process_running_elevated_(false) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::InitExtensionsClient();
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_BASIC_PRINTING) && defined(OS_WIN))
  handlers_.push_back(base::MakeUnique<printing::PrintingHandler>());
#endif
}

ChromeContentUtilityClient::~ChromeContentUtilityClient() = default;

void ChromeContentUtilityClient::UtilityThreadStarted() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::utility_handler::UtilityThreadStarted();
#endif

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUtilityProcessRunningElevated))
    utility_process_running_elevated_ = true;

  content::ServiceManagerConnection* connection =
      content::ChildThread::Get()->GetServiceManagerConnection();

  // NOTE: Some utility process instances are not connected to the Service
  // Manager. Nothing left to do in that case.
  if (!connection)
    return;

  auto registry = base::MakeUnique<service_manager::BinderRegistry>();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExposeInterfacesToBrowser(registry.get(),
                                        utility_process_running_elevated_);
  extensions::utility_handler::ExposeInterfacesToBrowser(
      registry.get(), utility_process_running_elevated_);
#endif
  // If our process runs with elevated privileges, only add elevated Mojo
  // interfaces to the interface registry.
  if (!utility_process_running_elevated_) {
    registry->AddInterface(base::Bind(&FilePatcherImpl::Create),
                           base::ThreadTaskRunnerHandle::Get());
#if !defined(OS_ANDROID)
    registry->AddInterface<net::interfaces::ProxyResolverFactory>(
        base::Bind(CreateProxyResolverFactory),
        base::ThreadTaskRunnerHandle::Get());
    registry->AddInterface(base::Bind(CreateResourceUsageReporter),
                           base::ThreadTaskRunnerHandle::Get());
    registry->AddInterface(
        base::Bind(&media_router::DialDeviceDescriptionParserImpl::Create),
        base::ThreadTaskRunnerHandle::Get());
#endif  // !defined(OS_ANDROID)
    registry->AddInterface(base::Bind(&payments::PaymentManifestParser::Create),
                           base::ThreadTaskRunnerHandle::Get());
    registry->AddInterface(
        base::Bind(&safe_json::SafeJsonParserMojoImpl::Create),
        base::ThreadTaskRunnerHandle::Get());
#if defined(OS_WIN)
    registry->AddInterface(base::Bind(&ShellHandlerImpl::Create),
                           base::ThreadTaskRunnerHandle::Get());
#endif
#if defined(OS_CHROMEOS)
    registry->AddInterface(base::Bind(&ZipFileCreatorImpl::Create),
                           base::ThreadTaskRunnerHandle::Get());
#endif
#if defined(FULL_SAFE_BROWSING)
    registry->AddInterface(base::Bind(&SafeArchiveAnalyzerImpl::Create),
                           base::ThreadTaskRunnerHandle::Get());
#endif
  }

  connection->AddConnectionFilter(
      base::MakeUnique<content::SimpleConnectionFilter>(std::move(registry)));
}

bool ChromeContentUtilityClient::OnMessageReceived(
    const IPC::Message& message) {
  if (utility_process_running_elevated_)
    return false;

  for (const auto& handler : handlers_) {
    if (handler->OnMessageReceived(message))
      return true;
  }

  return false;
}

void ChromeContentUtilityClient::RegisterServices(
    ChromeContentUtilityClient::StaticServiceMap* services) {
#if BUILDFLAG(ENABLE_PRINTING)
  service_manager::EmbeddedServiceInfo pdf_compositor_info;
  pdf_compositor_info.factory =
      base::Bind(&printing::CreatePdfCompositorService, GetUserAgent());
  services->emplace(printing::mojom::kServiceName, pdf_compositor_info);
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  service_manager::EmbeddedServiceInfo pdf_to_pwg_converter_info;
  pdf_to_pwg_converter_info.factory =
      base::Bind(&printing::PDFToPWGRasterConverterService::CreateService);
  services->emplace(printing::mojom::kPdfToPwgRasterConverterServiceName,
                    pdf_to_pwg_converter_info);
#endif

  service_manager::EmbeddedServiceInfo profiling_info;
  profiling_info.task_runner = content::ChildThread::Get()->GetIOTaskRunner();
  profiling_info.factory =
      base::Bind(&profiling::ProfilingService::CreateService);
  services->emplace(profiling::mojom::kServiceName, profiling_info);

#if !defined(OS_ANDROID)
  service_manager::EmbeddedServiceInfo profile_import_info;
  profile_import_info.factory =
      base::Bind(&ProfileImportService::CreateService);
  services->emplace(chrome::mojom::kProfileImportServiceName,
                    profile_import_info);
#endif

#if BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)
  RegisterMashServices(services);
#endif
}

void ChromeContentUtilityClient::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  if (g_network_binder_creation_callback.Get())
    g_network_binder_creation_callback.Get().Run(registry);
}

// static
void ChromeContentUtilityClient::PreSandboxStartup() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::PreSandboxStartup();
#endif
}

// static
void ChromeContentUtilityClient::SetNetworkBinderCreationCallback(
    const NetworkBinderCreationCallback& callback) {
  g_network_binder_creation_callback.Get() = callback;
}
