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
#include "media/media_features.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/data_decoder/public/interfaces/constants.mojom.h"
#include "services/shape_detection/public/interfaces/constants.mojom.h"
#include "services/shape_detection/shape_detection_service.h"
#include "services/video_capture/public/interfaces/constants.mojom.h"
#include "services/video_capture/service_impl.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "base/memory/ptr_util.h"
#include "media/base/scoped_callback_runner.h"
#include "media/cdm/cdm_adapter_factory.h"           // nogncheck
#include "media/cdm/cdm_helpers.h"                   // nogncheck
#include "media/mojo/features.h"                     // nogncheck
#include "media/mojo/interfaces/constants.mojom.h"   // nogncheck
#include "media/mojo/interfaces/output_protection.mojom.h"      // nogncheck
#include "media/mojo/interfaces/platform_verification.mojom.h"  // nogncheck
#include "media/mojo/services/media_service.h"       // nogncheck
#include "media/mojo/services/mojo_cdm_allocator.h"  // nogncheck
#include "media/mojo/services/mojo_media_client.h"   // nogncheck
#include "services/service_manager/public/cpp/connect.h"
#endif

namespace {

std::unique_ptr<service_manager::Service> CreateVideoCaptureService() {
  return base::MakeUnique<video_capture::ServiceImpl>();
}

}  // anonymous namespace

namespace content {

namespace {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

static_assert(BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE), "");
static_assert(BUILDFLAG(ENABLE_MOJO_CDM), "");

// Helper class that connects the CDM to various auxiliary services. All
// additional services (FileIO, memory allocation, output protection, and
// platform verification) are lazily created.
class MojoCdmHelper : public media::CdmAuxiliaryHelper {
 public:
  explicit MojoCdmHelper(
      service_manager::mojom::InterfaceProvider* interface_provider)
      : interface_provider_(interface_provider) {}
  ~MojoCdmHelper() override = default;

  // CdmAuxiliaryHelper implementation.
  std::unique_ptr<media::CdmFileIO> CreateCdmFileIO(
      cdm::FileIOClient* client) override {
    // TODO(jrummell): Hook up File IO. http://crbug.com/479923.
    return nullptr;
  }

  cdm::Buffer* CreateCdmBuffer(size_t capacity) override {
    return GetAllocator()->CreateCdmBuffer(capacity);
  }

  std::unique_ptr<media::VideoFrameImpl> CreateCdmVideoFrame() override {
    return GetAllocator()->CreateCdmVideoFrame();
  }

  void QueryStatus(QueryStatusCB callback) override {
    QueryStatusCB scoped_callback =
        media::ScopedCallbackRunner(std::move(callback), false, 0, 0);
    if (!ConnectToOutputProtection())
      return;

    output_protection_->QueryStatus(std::move(scoped_callback));
  }

  void EnableProtection(uint32_t desired_protection_mask,
                        EnableProtectionCB callback) override {
    EnableProtectionCB scoped_callback =
        media::ScopedCallbackRunner(std::move(callback), false);
    if (!ConnectToOutputProtection())
      return;

    output_protection_->EnableProtection(desired_protection_mask,
                                         std::move(scoped_callback));
  }

  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         ChallengePlatformCB callback) override {
    ChallengePlatformCB scoped_callback =
        media::ScopedCallbackRunner(std::move(callback), false, "", "", "");
    if (!ConnectToPlatformVerification())
      return;

    platform_verification_->ChallengePlatform(service_id, challenge,
                                              std::move(scoped_callback));
  }

  void GetStorageId(StorageIdCB callback) override {
    StorageIdCB scoped_callback = media::ScopedCallbackRunner(
        std::move(callback), std::vector<uint8_t>());
    // TODO(jrummell): Hook up GetStorageId() once added to the mojo interface.
    // http://crbug.com/478960.
  }

 private:
  // All services are created lazily.
  media::CdmAllocator* GetAllocator() {
    if (!allocator_)
      allocator_ = base::MakeUnique<media::MojoCdmAllocator>();
    return allocator_.get();
  }

  bool ConnectToOutputProtection() {
    if (!output_protection_attempted_) {
      output_protection_attempted_ = true;
      service_manager::GetInterface<media::mojom::OutputProtection>(
          interface_provider_, &output_protection_);
    }
    return output_protection_.is_bound();
  }

  bool ConnectToPlatformVerification() {
    if (!platform_verification_attempted_) {
      platform_verification_attempted_ = true;
      service_manager::GetInterface<media::mojom::PlatformVerification>(
          interface_provider_, &platform_verification_);
    }
    return platform_verification_.is_bound();
  }

  // Provides interfaces when needed.
  service_manager::mojom::InterfaceProvider* interface_provider_;

  // Keep track if connection to the Mojo service has been attempted once.
  // The service may not exist, or may fail later.
  bool output_protection_attempted_ = false;
  bool platform_verification_attempted_ = false;

  std::unique_ptr<media::CdmAllocator> allocator_;
  media::mojom::OutputProtectionPtr output_protection_;
  media::mojom::PlatformVerificationPtr platform_verification_;
};

std::unique_ptr<media::CdmAuxiliaryHelper> CreateCdmHelper(
    service_manager::mojom::InterfaceProvider* interface_provider) {
  return base::MakeUnique<MojoCdmHelper>(interface_provider);
}

class CdmMojoMediaClient final : public media::MojoMediaClient {
 public:
  CdmMojoMediaClient() {}
  ~CdmMojoMediaClient() override {}

  std::unique_ptr<media::CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces) override {
    return base::MakeUnique<media::CdmAdapterFactory>(
        base::Bind(&CreateCdmHelper, host_interfaces));
  }
};

std::unique_ptr<service_manager::Service> CreateCdmService() {
  return std::unique_ptr<service_manager::Service>(
      new ::media::MediaService(base::MakeUnique<CdmMojoMediaClient>()));
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

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

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
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
