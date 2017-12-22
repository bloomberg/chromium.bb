// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_service_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/network/network_service_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "content/utility/utility_thread_impl.h"
#include "media/media_features.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/data_decoder/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/shape_detection/public/interfaces/constants.mojom.h"
#include "services/shape_detection/shape_detection_service.h"
#include "services/video_capture/public/interfaces/constants.mojom.h"
#include "services/video_capture/service_impl.h"
#include "services/viz/public/interfaces/constants.mojom.h"
#include "services/viz/service.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_adapter_factory.h"           // nogncheck
#include "media/mojo/features.h"                     // nogncheck
#include "media/mojo/interfaces/constants.mojom.h"   // nogncheck
#include "media/mojo/services/cdm_service.h"         // nogncheck
#include "media/mojo/services/mojo_cdm_helper.h"     // nogncheck
#include "media/mojo/services/mojo_media_client.h"   // nogncheck
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "media/cdm/cdm_host_file.h"
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"

extern sandbox::TargetServices* g_utility_target_services;
#endif

namespace {

std::unique_ptr<service_manager::Service> CreateVideoCaptureService() {
  return std::make_unique<video_capture::ServiceImpl>();
}

}  // anonymous namespace

namespace content {

namespace {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

static_assert(BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE), "");
static_assert(BUILDFLAG(ENABLE_MOJO_CDM), "");

std::unique_ptr<media::CdmAuxiliaryHelper> CreateCdmHelper(
    service_manager::mojom::InterfaceProvider* interface_provider) {
  return std::make_unique<media::MojoCdmHelper>(interface_provider);
}

class CdmMojoMediaClient final : public media::MojoMediaClient {
 public:
  CdmMojoMediaClient() {}
  ~CdmMojoMediaClient() override {}

  void EnsureSandboxed() override {
#if defined(OS_WIN)
    // |g_utility_target_services| can be null if --no-sandbox is specified.
    if (g_utility_target_services)
      g_utility_target_services->LowerToken();
#endif
  }

  std::unique_ptr<media::CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces) override {
    return std::make_unique<media::CdmAdapterFactory>(
        base::Bind(&CreateCdmHelper, host_interfaces));
  }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  void AddCdmHostFilePaths(
      std::vector<media::CdmHostFilePath>* cdm_host_file_paths) override {
    GetContentClient()->AddContentDecryptionModules(nullptr,
                                                    cdm_host_file_paths);
  }
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
};

std::unique_ptr<service_manager::Service> CreateCdmService() {
  return std::unique_ptr<service_manager::Service>(
      new ::media::CdmService(std::make_unique<CdmMojoMediaClient>()));
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

std::unique_ptr<service_manager::Service> CreateDataDecoderService() {
  content::UtilityThread::Get()->EnsureBlinkInitialized();
  return data_decoder::DataDecoderService::Create();
}

std::unique_ptr<service_manager::Service> CreateVizService() {
  return std::make_unique<viz::Service>();
}

}  // namespace

UtilityServiceFactory::UtilityServiceFactory()
    : network_registry_(std::make_unique<service_manager::BinderRegistry>()) {}

UtilityServiceFactory::~UtilityServiceFactory() {}

void UtilityServiceFactory::CreateService(
    service_manager::mojom::ServiceRequest request,
    const std::string& name) {
  auto* trace_log = base::trace_event::TraceLog::GetInstance();
  if (trace_log->IsProcessNameEmpty())
    trace_log->set_process_name("Service: " + name);
  ServiceFactory::CreateService(std::move(request), name);
}

void UtilityServiceFactory::RegisterServices(ServiceMap* services) {
  GetContentClient()->utility()->RegisterServices(services);

  service_manager::EmbeddedServiceInfo video_capture_info;
  video_capture_info.factory = base::Bind(&CreateVideoCaptureService);
  services->insert(
      std::make_pair(video_capture::mojom::kServiceName, video_capture_info));

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  service_manager::EmbeddedServiceInfo info;
  info.factory = base::Bind(&CreateCdmService);
  services->emplace(media::mojom::kCdmServiceName, info);
#endif

  service_manager::EmbeddedServiceInfo shape_detection_info;
  shape_detection_info.factory =
      base::Bind(&shape_detection::ShapeDetectionService::Create);
  services->insert(std::make_pair(shape_detection::mojom::kServiceName,
                                  shape_detection_info));

  service_manager::EmbeddedServiceInfo data_decoder_info;
  data_decoder_info.factory = base::Bind(&CreateDataDecoderService);
  services->insert(
      std::make_pair(data_decoder::mojom::kServiceName, data_decoder_info));

  service_manager::EmbeddedServiceInfo viz_info;
  viz_info.factory = base::Bind(&CreateVizService);
  services->insert(std::make_pair(viz::mojom::kVizServiceName, viz_info));

  if (base::FeatureList::IsEnabled(features::kNetworkService)) {
    GetContentClient()->utility()->RegisterNetworkBinders(
        network_registry_.get());
    service_manager::EmbeddedServiceInfo network_info;
    network_info.factory = base::Bind(
        &UtilityServiceFactory::CreateNetworkService, base::Unretained(this));
    network_info.task_runner = ChildProcess::current()->io_task_runner();
    services->insert(
        std::make_pair(content::mojom::kNetworkServiceName, network_info));
  }
}

void UtilityServiceFactory::OnServiceQuit() {
  UtilityThread::Get()->ReleaseProcess();
}

void UtilityServiceFactory::OnLoadFailed() {
  UtilityThreadImpl* utility_thread =
      static_cast<UtilityThreadImpl*>(UtilityThread::Get());
  utility_thread->Shutdown();
  utility_thread->ReleaseProcess();
}

std::unique_ptr<service_manager::Service>
UtilityServiceFactory::CreateNetworkService() {
  return std::make_unique<NetworkServiceImpl>(std::move(network_registry_));
}

}  // namespace content
