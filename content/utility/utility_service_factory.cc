// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_service_factory.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "content/utility/utility_thread_impl.h"
#include "media/media_buildflags.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/service_factory.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/service_impl.h"
#include "services/viz/public/interfaces/constants.mojom.h"
#include "services/viz/service.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_adapter_factory.h"           // nogncheck
#include "media/mojo/interfaces/constants.mojom.h"   // nogncheck
#include "media/mojo/services/cdm_service.h"         // nogncheck
#include "media/mojo/services/mojo_cdm_helper.h"     // nogncheck
#include "media/mojo/services/mojo_media_client.h"   // nogncheck
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "media/cdm/cdm_host_file.h"
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#endif

#if defined(OS_MACOSX)
#include "sandbox/mac/system_services.h"
#include "services/service_manager/sandbox/features.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"

extern sandbox::TargetServices* g_utility_target_services;
#endif

namespace content {

namespace {

void TerminateThisProcess() {
  UtilityThread::Get()->ReleaseProcess();
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

std::unique_ptr<media::CdmAuxiliaryHelper> CreateCdmHelper(
    service_manager::mojom::InterfaceProvider* interface_provider) {
  return std::make_unique<media::MojoCdmHelper>(interface_provider);
}

class ContentCdmServiceClient final : public media::CdmService::Client {
 public:
  ContentCdmServiceClient() {}
  ~ContentCdmServiceClient() override {}

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

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

}  // namespace

UtilityServiceFactory::UtilityServiceFactory()
    : network_registry_(std::make_unique<service_manager::BinderRegistry>()),
      audio_registry_(std::make_unique<service_manager::BinderRegistry>()) {}

UtilityServiceFactory::~UtilityServiceFactory() {}

void UtilityServiceFactory::CreateService(
    service_manager::mojom::ServiceRequest request,
    const std::string& name,
    service_manager::mojom::PIDReceiverPtr pid_receiver) {
  auto* trace_log = base::trace_event::TraceLog::GetInstance();
  if (trace_log->IsProcessNameEmpty())
    trace_log->set_process_name("Service: " + name);

  static auto* service_name = base::debug::AllocateCrashKeyString(
      "service-name", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(service_name, name);

  ServiceFactory::CreateService(std::move(request), name,
                                std::move(pid_receiver));
}

void UtilityServiceFactory::RegisterServices(ServiceMap* services) {
  GetContentClient()->utility()->RegisterServices(services);

  GetContentClient()->utility()->RegisterAudioBinders(audio_registry_.get());

}

bool UtilityServiceFactory::HandleServiceRequest(
    const std::string& name,
    service_manager::mojom::ServiceRequest request) {
  if (name == audio::mojom::kServiceName) {
    running_service_ = CreateAudioService(std::move(request));
  } else if (name == data_decoder::mojom::kServiceName) {
    content::UtilityThread::Get()->EnsureBlinkInitialized();
    running_service_ =
        std::make_unique<data_decoder::DataDecoderService>(std::move(request));
  } else if (name == mojom::kNetworkServiceName &&
             base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // Unlike other services supported by the utility process, the network
    // service runs on the IO thread and never self-terminates.
    GetContentClient()->utility()->RegisterNetworkBinders(
        network_registry_.get());
    ChildProcess::current()->io_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&UtilityServiceFactory::RunNetworkServiceOnIOThread,
                       base::Unretained(this), std::move(request),
                       base::SequencedTaskRunnerHandle::Get()));
    return true;
  } else if (name == video_capture::mojom::kServiceName) {
    running_service_ =
        std::make_unique<video_capture::ServiceImpl>(std::move(request));
  } else if (name == viz::mojom::kVizServiceName) {
    running_service_ = std::make_unique<viz::Service>(std::move(request));
  }
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  else if (name == media::mojom::kCdmServiceName) {
    running_service_ = std::make_unique<media::CdmService>(
        std::make_unique<ContentCdmServiceClient>(), std::move(request));
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  if (!running_service_) {
    running_service_ = GetContentClient()->utility()->HandleServiceRequest(
        name, std::move(request));
  }

  if (running_service_) {
    // If we actually started a service for this request, make sure its
    // self-termination results in full process termination.
    running_service_->set_termination_closure(
        base::BindOnce(&TerminateThisProcess));
    return true;
  }

  return false;
}

void UtilityServiceFactory::OnServiceQuit() {
  TerminateThisProcess();
}

void UtilityServiceFactory::OnLoadFailed() {
  UtilityThreadImpl* utility_thread =
      static_cast<UtilityThreadImpl*>(UtilityThread::Get());
  utility_thread->Shutdown();
  utility_thread->ReleaseProcess();
}

void UtilityServiceFactory::RunNetworkServiceOnIOThread(
    service_manager::mojom::ServiceRequest service_request,
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner) {
  auto service = std::make_unique<network::NetworkService>(
      std::move(network_registry_), nullptr /* request */,
      nullptr /* net_log */, std::move(service_request));

  // Transfer ownership of the service to itself, and have it post to the main
  // thread on self-termination to kill the process.
  auto* raw_service = service.get();
  raw_service->set_termination_closure(base::BindOnce(
      [](std::unique_ptr<network::NetworkService> service,
         scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner) {
        main_thread_task_runner->PostTask(
            FROM_HERE, base::BindOnce(&TerminateThisProcess));
      },
      std::move(service), std::move(main_thread_task_runner)));
}

std::unique_ptr<service_manager::Service>
UtilityServiceFactory::CreateAudioService(
    service_manager::mojom::ServiceRequest request) {
#if defined(OS_MACOSX)
  // Don't connect to launch services when running sandboxed
  // (https://crbug.com/874785).
  if (base::FeatureList::IsEnabled(
          service_manager::features::kAudioServiceSandbox)) {
    sandbox::DisableLaunchServices();
  }
#endif

  return audio::CreateStandaloneService(std::move(audio_registry_),
                                        std::move(request));
}

}  // namespace content
