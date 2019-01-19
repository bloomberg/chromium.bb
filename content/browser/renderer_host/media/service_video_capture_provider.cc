// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_provider.h"

#include "base/task/post_task.h"
#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"
#include "content/browser/renderer_host/media/virtual_video_capture_devices_changed_observer.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/delegate_to_browser_gpu_service_accelerator_factory.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"

namespace {

std::unique_ptr<video_capture::mojom::AcceleratorFactory>
CreateAcceleratorFactory() {
  return std::make_unique<
      content::DelegateToBrowserGpuServiceAcceleratorFactory>();
}

}  // anonymous namespace

namespace content {

ServiceVideoCaptureProvider::ServiceVideoCaptureProvider(
    service_manager::Connector* connector,
    base::RepeatingCallback<void(const std::string&)> emit_log_message_cb)
    : ServiceVideoCaptureProvider(
          base::BindRepeating(&CreateAcceleratorFactory),
          connector,
          std::move(emit_log_message_cb)) {}

ServiceVideoCaptureProvider::ServiceVideoCaptureProvider(
    CreateAcceleratorFactoryCallback create_accelerator_factory_cb,
    service_manager::Connector* connector,
    base::RepeatingCallback<void(const std::string&)> emit_log_message_cb)
    : connector_(connector ? connector->Clone() : nullptr),
      create_accelerator_factory_cb_(std::move(create_accelerator_factory_cb)),
      emit_log_message_cb_(std::move(emit_log_message_cb)),
      launcher_has_connected_to_device_factory_(false),
      service_listener_binding_(this),
      weak_ptr_factory_(this) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &ServiceVideoCaptureProvider::RegisterServiceListenerOnIOThread,
          weak_ptr_factory_.GetWeakPtr()));
}

ServiceVideoCaptureProvider::~ServiceVideoCaptureProvider() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  OnServiceConnectionClosed(ReasonForDisconnect::kShutdown);
}

void ServiceVideoCaptureProvider::GetDeviceInfosAsync(
    GetDeviceInfosCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  emit_log_message_cb_.Run("ServiceVideoCaptureProvider::GetDeviceInfosAsync");
  auto service_connection = LazyConnectToService();
  // Use a ScopedCallbackRunner to make sure that |result_callback| gets
  // invoked with an empty result in case that the service drops the request.
  service_connection->device_factory()->GetDeviceInfos(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&ServiceVideoCaptureProvider::OnDeviceInfosReceived,
                         weak_ptr_factory_.GetWeakPtr(), service_connection,
                         std::move(result_callback)),
          std::vector<media::VideoCaptureDeviceInfo>()));
}

std::unique_ptr<VideoCaptureDeviceLauncher>
ServiceVideoCaptureProvider::CreateDeviceLauncher() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return std::make_unique<ServiceVideoCaptureDeviceLauncher>(
      base::BindRepeating(
          &ServiceVideoCaptureProvider::OnLauncherConnectingToDeviceFactory,
          weak_ptr_factory_.GetWeakPtr()));
}

void ServiceVideoCaptureProvider::OnServiceStarted(
    const ::service_manager::Identity& identity,
    uint32_t pid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (identity.name() != video_capture::mojom::kServiceName)
    return;

  // Whenever the video capture service starts, we register a
  // VirtualVideoCaptureDevicesChangedObserver in order to propagate device
  // change events when virtual devices are added to or removed from the
  // service.
  auto service_connection = LazyConnectToService();
  video_capture::mojom::DevicesChangedObserverPtr observer;
  mojo::MakeStrongBinding(
      std::make_unique<VirtualVideoCaptureDevicesChangedObserver>(),
      mojo::MakeRequest(&observer));
  service_connection->device_factory()->RegisterVirtualDevicesChangedObserver(
      std::move(observer),
      true /*raise_event_if_virtual_devices_already_present*/);
}

void ServiceVideoCaptureProvider::RegisterServiceListenerOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!connector_)
    return;

  service_manager::mojom::ServiceManagerListenerPtr listener;
  service_listener_binding_.Bind(mojo::MakeRequest(&listener));

  service_manager::mojom::ServiceManagerPtr service_manager;
  connector_->BindInterface(service_manager::mojom::kServiceName,
                            &service_manager);
  service_manager->AddListener(std::move(listener));
}

void ServiceVideoCaptureProvider::OnLauncherConnectingToDeviceFactory(
    scoped_refptr<RefCountedVideoCaptureFactory>* out_factory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  launcher_has_connected_to_device_factory_ = true;
  *out_factory = LazyConnectToService();
}

scoped_refptr<RefCountedVideoCaptureFactory>
ServiceVideoCaptureProvider::LazyConnectToService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (weak_service_connection_) {
    // There already is a connection.
    return base::WrapRefCounted(weak_service_connection_.get());
  }

  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::BROWSER_CONNECTING_TO_SERVICE);
  if (time_of_last_uninitialize_ != base::TimeTicks()) {
    if (launcher_has_connected_to_device_factory_) {
      video_capture::uma::LogDurationUntilReconnectAfterCapture(
          base::TimeTicks::Now() - time_of_last_uninitialize_);
    } else {
      video_capture::uma::LogDurationUntilReconnectAfterEnumerationOnly(
          base::TimeTicks::Now() - time_of_last_uninitialize_);
    }
  }

  launcher_has_connected_to_device_factory_ = false;
  time_of_last_connect_ = base::TimeTicks::Now();

  video_capture::mojom::AcceleratorFactoryPtr accelerator_factory;
  mojo::MakeStrongBinding(create_accelerator_factory_cb_.Run(),
                          mojo::MakeRequest(&accelerator_factory));

  DCHECK(connector_)
      << "Attempted to connect to the video capture service from "
         "a process that does not provide a "
         "ServiceManagerConnection";
  video_capture::mojom::DeviceFactoryProviderPtr device_factory_provider;
  connector_->BindInterface(video_capture::mojom::kServiceName,
                            &device_factory_provider);

  device_factory_provider->InjectGpuDependencies(
      std::move(accelerator_factory));
  video_capture::mojom::DeviceFactoryPtr device_factory;
  device_factory_provider->ConnectToDeviceFactory(
      mojo::MakeRequest(&device_factory));
  device_factory.set_connection_error_handler(base::BindOnce(
      &ServiceVideoCaptureProvider::OnLostConnectionToDeviceFactory,
      weak_ptr_factory_.GetWeakPtr()));
  auto result = base::MakeRefCounted<RefCountedVideoCaptureFactory>(
      std::move(device_factory),
      base::BindOnce(&ServiceVideoCaptureProvider::OnServiceConnectionClosed,
                     weak_ptr_factory_.GetWeakPtr(),
                     ReasonForDisconnect::kUnused));
  weak_service_connection_ = result->GetWeakPtr();
  return result;
}

void ServiceVideoCaptureProvider::OnDeviceInfosReceived(
    scoped_refptr<RefCountedVideoCaptureFactory> service_connection,
    GetDeviceInfosCallback result_callback,
    const std::vector<media::VideoCaptureDeviceInfo>& infos) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::ResetAndReturn(&result_callback).Run(infos);
}

void ServiceVideoCaptureProvider::OnLostConnectionToDeviceFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  emit_log_message_cb_.Run(
      "ServiceVideoCaptureProvider::OnLostConnectionToDeviceFactory");
  // This may indicate that the video capture service has crashed. Uninitialize
  // here, so that a new connection will be established when clients try to
  // reconnect.
  OnServiceConnectionClosed(ReasonForDisconnect::kConnectionLost);
}

void ServiceVideoCaptureProvider::OnServiceConnectionClosed(
    ReasonForDisconnect reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::TimeDelta duration_since_last_connect(base::TimeTicks::Now() -
                                              time_of_last_connect_);
  switch (reason) {
    case ReasonForDisconnect::kShutdown:
    case ReasonForDisconnect::kUnused:
      if (launcher_has_connected_to_device_factory_) {
        video_capture::uma::LogVideoCaptureServiceEvent(
            video_capture::uma::
                BROWSER_CLOSING_CONNECTION_TO_SERVICE_AFTER_CAPTURE);
        video_capture::uma::
            LogDurationFromLastConnectToClosingConnectionAfterCapture(
                duration_since_last_connect);
      } else {
        video_capture::uma::LogVideoCaptureServiceEvent(
            video_capture::uma::
                BROWSER_CLOSING_CONNECTION_TO_SERVICE_AFTER_ENUMERATION_ONLY);
        video_capture::uma::
            LogDurationFromLastConnectToClosingConnectionAfterEnumerationOnly(
                duration_since_last_connect);
      }
      break;
    case ReasonForDisconnect::kConnectionLost:
      video_capture::uma::LogVideoCaptureServiceEvent(
          video_capture::uma::BROWSER_LOST_CONNECTION_TO_SERVICE);
      video_capture::uma::LogDurationFromLastConnectToConnectionLost(
          duration_since_last_connect);
      break;
  }
  time_of_last_uninitialize_ = base::TimeTicks::Now();
}

}  // namespace content
