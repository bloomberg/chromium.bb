// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_service_factory.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/child/child_process.h"
#include "content/network/network_service_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "content/utility/utility_thread_impl.h"
#include "ppapi/features/features.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/data_decoder/public/interfaces/constants.mojom.h"
#include "services/shape_detection/public/interfaces/constants.mojom.h"
#include "services/shape_detection/shape_detection_service.h"
#include "services/video_capture/public/interfaces/constants.mojom.h"
#include "services/video_capture/service_impl.h"

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
#include "base/memory/ptr_util.h"
#include "media/cdm/cdm_adapter_factory.h"           // nogncheck
#include "media/mojo/features.h"                     // nogncheck
#include "media/mojo/interfaces/constants.mojom.h"   // nogncheck
#include "media/mojo/services/media_service.h"       // nogncheck
#include "media/mojo/services/mojo_cdm_allocator.h"  // nogncheck
#include "media/mojo/services/mojo_media_client.h"   // nogncheck
#endif

namespace {

std::unique_ptr<service_manager::Service> CreateVideoCaptureService() {
  return base::MakeUnique<video_capture::ServiceImpl>();
}

}  // anonymous namespace

namespace content {

namespace {

#if BUILDFLAG(ENABLE_PEPPER_CDMS)

static_assert(BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE), "");
static_assert(BUILDFLAG(ENABLE_MOJO_CDM), "");

std::unique_ptr<media::CdmAllocator> CreateCdmAllocator() {
  return base::MakeUnique<media::MojoCdmAllocator>();
}

class CdmMojoMediaClient final : public media::MojoMediaClient {
 public:
  CdmMojoMediaClient() {}
  ~CdmMojoMediaClient() override {}

  std::unique_ptr<media::CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces) override {
    return base::MakeUnique<media::CdmAdapterFactory>(
        base::Bind(&CreateCdmAllocator));
  }
};

std::unique_ptr<service_manager::Service> CreateCdmService() {
  return std::unique_ptr<service_manager::Service>(
      new ::media::MediaService(base::MakeUnique<CdmMojoMediaClient>()));
}
#endif  // BUILDFLAG(ENABLE_PEPPER_CDMS)

std::unique_ptr<service_manager::Service> CreateDataDecoderService() {
  content::UtilityThread::Get()->EnsureBlinkInitialized();
  return data_decoder::DataDecoderService::Create();
}

}  // namespace

UtilityServiceFactory::UtilityServiceFactory()
    : network_registry_(base::MakeUnique<service_manager::BinderRegistry>()) {}

UtilityServiceFactory::~UtilityServiceFactory() {}

void UtilityServiceFactory::RegisterServices(ServiceMap* services) {
  GetContentClient()->utility()->RegisterServices(services);

  service_manager::EmbeddedServiceInfo video_capture_info;
  video_capture_info.factory = base::Bind(&CreateVideoCaptureService);
  services->insert(
      std::make_pair(video_capture::mojom::kServiceName, video_capture_info));

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
  service_manager::EmbeddedServiceInfo info;
  info.factory = base::Bind(&CreateCdmService);
  services->insert(std::make_pair(media::mojom::kCdmServiceName, info));
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
  return base::MakeUnique<NetworkServiceImpl>(std::move(network_registry_));
}

}  // namespace content
