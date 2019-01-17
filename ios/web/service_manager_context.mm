// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/service_manager_context.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "ios/web/public/service_manager_connection.h"
#include "ios/web/public/service_names.mojom.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/service_manager_connection_impl.h"
#import "ios/web/web_browser_manifest.h"
#import "ios/web/web_packaged_services_manifest.h"
#include "services/catalog/public/mojom/constants.mojom.h"
#include "services/service_manager/connect_params.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/service_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

struct ManifestInfo {
  const char* name;
  int resource_id;
};

}  // namespace

// State which lives on the IO thread and drives the ServiceManager.
class ServiceManagerContext::InProcessServiceManagerContext
    : public base::RefCountedThreadSafe<InProcessServiceManagerContext> {
 public:
  InProcessServiceManagerContext() {}

  void Start(
      service_manager::mojom::ServicePtrInfo packaged_services_service_info,
      std::vector<service_manager::Manifest> manifests) {
    base::PostTaskWithTraits(
        FROM_HERE, {WebThread::IO},
        base::BindOnce(&InProcessServiceManagerContext::StartOnIOThread, this,
                       std::move(manifests),
                       std::move(packaged_services_service_info)));
  }

  void ShutDown() {
    base::PostTaskWithTraits(
        FROM_HERE, {WebThread::IO},
        base::BindOnce(&InProcessServiceManagerContext::ShutDownOnIOThread,
                       this));
  }

 private:
  friend class base::RefCountedThreadSafe<InProcessServiceManagerContext>;

  ~InProcessServiceManagerContext() {}

  // Creates the ServiceManager and registers the packaged services service
  // with it, connecting the other end of the packaged services serviceto
  // |packaged_services_service_info|.
  void StartOnIOThread(
      std::vector<service_manager::Manifest> manifests,
      service_manager::mojom::ServicePtrInfo packaged_services_service_info) {
    service_manager_ =
        std::make_unique<service_manager::ServiceManager>(nullptr, manifests);

    service_manager::mojom::ServicePtr packaged_services_service;
    packaged_services_service.Bind(std::move(packaged_services_service_info));
    service_manager_->RegisterService(
        service_manager::Identity(mojom::kPackagedServicesServiceName,
                                  service_manager::kSystemInstanceGroup,
                                  base::Token{}, base::Token::CreateRandom()),
        std::move(packaged_services_service), nullptr);
  }

  void ShutDownOnIOThread() {
    service_manager_.reset();
  }

  std::unique_ptr<service_manager::ServiceManager> service_manager_;

  DISALLOW_COPY_AND_ASSIGN(InProcessServiceManagerContext);
};

ServiceManagerContext::ServiceManagerContext() {
  const std::vector<service_manager::Manifest> manifests = {
      GetWebBrowserManifest(), GetWebPackagedServicesManifest()};

  in_process_context_ = base::MakeRefCounted<InProcessServiceManagerContext>();
  service_manager::mojom::ServicePtr packaged_services_service;
  service_manager::mojom::ServiceRequest packaged_services_request =
      mojo::MakeRequest(&packaged_services_service);
  in_process_context_->Start(packaged_services_service.PassInterface(),
                             std::move(manifests));

  packaged_services_connection_ = ServiceManagerConnection::Create(
      std::move(packaged_services_request),
      base::CreateSingleThreadTaskRunnerWithTraits({WebThread::IO}));
  packaged_services_connection_->SetDefaultServiceRequestHandler(
      base::BindRepeating(&ServiceManagerContext::OnUnhandledServiceRequest,
                          weak_ptr_factory_.GetWeakPtr()));

  service_manager::mojom::ServicePtr root_browser_service;
  ServiceManagerConnection::Set(ServiceManagerConnection::Create(
      mojo::MakeRequest(&root_browser_service),
      base::CreateSingleThreadTaskRunnerWithTraits({WebThread::IO})));
  auto* browser_connection = ServiceManagerConnection::Get();

  service_manager::mojom::PIDReceiverPtr pid_receiver;
  packaged_services_connection_->GetConnector()->RegisterServiceInstance(
      service_manager::Identity(mojom::kBrowserServiceName,
                                service_manager::kSystemInstanceGroup,
                                base::Token{}, base::Token::CreateRandom()),
      std::move(root_browser_service), mojo::MakeRequest(&pid_receiver));
  pid_receiver->SetPID(base::GetCurrentProcId());

  packaged_services_connection_->Start();
  browser_connection->Start();
}

ServiceManagerContext::~ServiceManagerContext() {
  // NOTE: The in-process ServiceManager MUST be destroyed before the browser
  // process-wide ServiceManagerConnection. Otherwise it's possible for the
  // ServiceManager to receive connection requests for service:ios_web_browser
  // which it may attempt to service by launching a new instance of the browser.
  if (in_process_context_)
    in_process_context_->ShutDown();
  if (ServiceManagerConnection::Get())
    ServiceManagerConnection::Destroy();
}

void ServiceManagerContext::OnUnhandledServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  std::unique_ptr<service_manager::Service> service =
      GetWebClient()->HandleServiceRequest(service_name, std::move(request));
  if (!service) {
    LOG(ERROR) << "Ignoring unhandled request for service: " << service_name;
    return;
  }

  auto* raw_service = service.get();
  service->set_termination_closure(
      base::BindOnce(&ServiceManagerContext::OnServiceQuit,
                     base::Unretained(this), raw_service));
  running_services_.emplace(raw_service, std::move(service));
}

void ServiceManagerContext::OnServiceQuit(service_manager::Service* service) {
  running_services_.erase(service);
}

}  // namespace web
