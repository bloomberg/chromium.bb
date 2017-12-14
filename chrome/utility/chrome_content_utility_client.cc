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
#include "chrome/common/profiling/constants.mojom.h"
#include "chrome/profiling/profiling_service.h"
#include "chrome/utility/utility_message_handler.h"
#include "components/patch_service/patch_service.h"
#include "components/patch_service/public/interfaces/constants.mojom.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/utility/utility_thread.h"
#include "extensions/features/features.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "printing/features/features.h"
#include "services/service_manager/embedder/embedded_service_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/sandbox/switches.h"
#include "ui/base/ui_features.h"

#if !defined(OS_ANDROID)
#include "chrome/common/resource_usage_reporter.mojom.h"
#include "chrome/utility/importer/profile_import_impl.h"
#include "chrome/utility/importer/profile_import_service.h"
#include "content/public/network/url_request_context_builder_mojo.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "services/proxy_resolver/proxy_resolver_service.h"  // nogncheck
#include "services/proxy_resolver/public/interfaces/proxy_resolver.mojom.h"  // nogncheck
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)
#include "chrome/services/util_win/public/interfaces/constants.mojom.h"
#include "chrome/services/util_win/util_win_service.h"
#include "chrome/utility/printing/pdf_to_emf_converter_factory_impl.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/services/media_gallery_util/media_gallery_util_service.h"
#include "chrome/services/media_gallery_util/public/interfaces/constants.mojom.h"
#include "chrome/services/removable_storage_writer/public/interfaces/constants.mojom.h"
#include "chrome/services/removable_storage_writer/removable_storage_writer_service.h"
#include "chrome/utility/extensions/extensions_handler.h"
#include "extensions/utility/utility_handler.h"
#if defined(OS_WIN)
#include "chrome/services/wifi_util_win/public/interfaces/constants.mojom.h"
#include "chrome/services/wifi_util_win/wifi_util_win_service.h"
#endif
#endif

#if BUILDFLAG(ENABLE_MUS)
#include "chrome/utility/mash_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/services/printing/printing_service.h"
#include "chrome/services/printing/public/interfaces/constants.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_BASIC_PRINTING) && defined(OS_WIN))
#include "chrome/utility/printing_handler.h"
#endif

#if defined(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
#include "chrome/services/file_util/file_util_service.h"  // nogncheck
#include "chrome/services/file_util/public/interfaces/constants.mojom.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/common/chrome_content_client.h"
#include "components/printing/service/public/cpp/pdf_compositor_service_factory.h"  // nogncheck
#include "components/printing/service/public/interfaces/pdf_compositor.mojom.h"  // nogncheck
#endif

namespace {

#if !defined(OS_ANDROID)
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
void RegisterRemovableStorageWriterService(
    ChromeContentUtilityClient::StaticServiceMap* services) {
  service_manager::EmbeddedServiceInfo service_info;
  service_info.factory =
      base::Bind(&chrome::RemovableStorageWriterService::CreateService);
  services->emplace(chrome::mojom::kRemovableStorageWriterServiceName,
                    service_info);
}
#endif

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

#if defined(OS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  utility_process_running_elevated_ = command_line->HasSwitch(
      service_manager::switches::kNoSandboxAndElevatedPrivileges);
#endif

  content::ServiceManagerConnection* connection =
      content::ChildThread::Get()->GetServiceManagerConnection();

  // NOTE: Some utility process instances are not connected to the Service
  // Manager. Nothing left to do in that case.
  if (!connection)
    return;

  auto registry = base::MakeUnique<service_manager::BinderRegistry>();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::utility_handler::ExposeInterfacesToBrowser(
      registry.get(), utility_process_running_elevated_);
#endif
  // If our process runs with elevated privileges, only add elevated Mojo
  // interfaces to the interface registry.
  if (!utility_process_running_elevated_) {
#if !defined(OS_ANDROID)
    registry->AddInterface(base::Bind(CreateResourceUsageReporter),
                           base::ThreadTaskRunnerHandle::Get());
#endif  // !defined(OS_ANDROID)
#if defined(OS_WIN)
    registry->AddInterface(
        base::Bind(printing::PdfToEmfConverterFactoryImpl::Create),
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
  if (utility_process_running_elevated_) {
#if defined(OS_WIN) && BUILDFLAG(ENABLE_EXTENSIONS)
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory =
        base::Bind(&chrome::WifiUtilWinService::CreateService);
    services->emplace(chrome::mojom::kWifiUtilWinServiceName, service_info);

    RegisterRemovableStorageWriterService(services);
#endif
    return;
  }

#if BUILDFLAG(ENABLE_PRINTING)
  service_manager::EmbeddedServiceInfo pdf_compositor_info;
  pdf_compositor_info.factory =
      base::Bind(&printing::CreatePdfCompositorService, GetUserAgent());
  services->emplace(printing::mojom::kServiceName, pdf_compositor_info);
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  {
    service_manager::EmbeddedServiceInfo printing_info;
    printing_info.factory =
        base::Bind(&printing::PrintingService::CreateService);
    services->emplace(printing::mojom::kChromePrintingServiceName,
                      printing_info);
  }
#endif

  service_manager::EmbeddedServiceInfo profiling_info;
  profiling_info.task_runner = content::ChildThread::Get()->GetIOTaskRunner();
  profiling_info.factory =
      base::Bind(&profiling::ProfilingService::CreateService);
  services->emplace(profiling::mojom::kServiceName, profiling_info);

#if !defined(OS_ANDROID)
  service_manager::EmbeddedServiceInfo proxy_resolver_info;
  proxy_resolver_info.task_runner =
      content::ChildThread::Get()->GetIOTaskRunner();
  proxy_resolver_info.factory =
      base::Bind(&proxy_resolver::ProxyResolverService::CreateService);
  services->emplace(proxy_resolver::mojom::kProxyResolverServiceName,
                    proxy_resolver_info);

  service_manager::EmbeddedServiceInfo profile_import_info;
  profile_import_info.factory =
      base::Bind(&ProfileImportService::CreateService);
  services->emplace(chrome::mojom::kProfileImportServiceName,
                    profile_import_info);
#endif

#if defined(OS_WIN)
  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory = base::Bind(&chrome::UtilWinService::CreateService);
    services->emplace(chrome::mojom::kUtilWinServiceName, service_info);
  }
#endif

#if defined(FULL_SAFE_BROWSING) || defined(OS_CHROMEOS)
  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory = base::Bind(&chrome::FileUtilService::CreateService);
    services->emplace(chrome::mojom::kFileUtilServiceName, service_info);
  }
#endif

  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory = base::Bind(&patch::PatchService::CreateService);
    services->emplace(patch::mojom::kServiceName, service_info);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  {
    service_manager::EmbeddedServiceInfo service_info;
    service_info.factory =
        base::Bind(&chrome::MediaGalleryUtilService::CreateService);
    services->emplace(chrome::mojom::kMediaGalleryUtilServiceName,
                      service_info);
  }
#if !defined(OS_WIN)
  // On Windows the service is running elevated.
  RegisterRemovableStorageWriterService(services);
#endif
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_MUS)
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
