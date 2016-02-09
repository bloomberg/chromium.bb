// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/devices_app/devices_app.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "device/core/device_client.h"
#include "device/devices_app/usb/device_manager_impl.h"
#include "device/usb/usb_service.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/shell.h"
#include "url/gurl.h"

namespace device {

namespace {

// The number of seconds to wait without any bound DeviceManagers before
// exiting the app.
const int64_t kIdleTimeoutInSeconds = 10;

// A DeviceClient implementation to be constructed iff the app is not running
// in an embedder that provides a DeviceClient (i.e. running as a standalone
// Mojo app, not in Chrome).
class AppDeviceClient : public DeviceClient {
 public:
  explicit AppDeviceClient(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
      : blocking_task_runner_(blocking_task_runner) {}
  ~AppDeviceClient() override {}

 private:
  // DeviceClient:
  UsbService* GetUsbService() override {
    if (!usb_service_) {
      usb_service_ = UsbService::Create(blocking_task_runner_);
    }
    return usb_service_.get();
  }

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<UsbService> usb_service_;
};

}  // namespace

// This class insures that a UsbService has been initialized and is accessible
// via the DeviceClient interface.
class DevicesApp::USBServiceInitializer {
 public:
  USBServiceInitializer()
      : blocking_thread_("USB service blocking I/O thread") {
    blocking_thread_.Start();
    app_device_client_.reset(
        new AppDeviceClient(blocking_thread_.task_runner()));
  }

  ~USBServiceInitializer() {}

 private:
  scoped_ptr<AppDeviceClient> app_device_client_;
  base::Thread blocking_thread_;

  DISALLOW_COPY_AND_ASSIGN(USBServiceInitializer);
};

DevicesApp::DevicesApp() : shell_(nullptr), active_device_manager_count_(0) {}

DevicesApp::~DevicesApp() {
}

void DevicesApp::Initialize(mojo::Shell* shell,
                            const std::string& url,
                            uint32_t id) {
  shell_ = shell;
  service_initializer_.reset(new USBServiceInitializer);
  StartIdleTimer();
}

bool DevicesApp::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<usb::DeviceManager>(this);
  return true;
}

void DevicesApp::Quit() {
  service_initializer_.reset();
  shell_ = nullptr;
}

void DevicesApp::Create(mojo::Connection* connection,
                        mojo::InterfaceRequest<usb::DeviceManager> request) {
  // Bind the new device manager to the connecting application's permission
  // provider.
  usb::PermissionProviderPtr permission_provider;
  connection->GetInterface(&permission_provider);

  // Owned by its message pipe.
  usb::DeviceManagerImpl* device_manager = new usb::DeviceManagerImpl(
      std::move(permission_provider), std::move(request));
  device_manager->set_connection_error_handler(
      base::Bind(&DevicesApp::OnConnectionError, base::Unretained(this)));

  active_device_manager_count_++;
  idle_timeout_callback_.Cancel();
}

void DevicesApp::OnConnectionError() {
  DCHECK_GE(active_device_manager_count_, 0u);
  active_device_manager_count_--;
  if (active_device_manager_count_ == 0) {
    // If the last DeviceManager connection has been dropped, kick off an idle
    // timeout to shut ourselves down.
    StartIdleTimer();
  }
}

void DevicesApp::StartIdleTimer() {
  // Passing unretained |shell_| is safe here because |shell_| is
  // guaranteed to outlive |this|, and the callback is canceled if |this| is
  // destroyed.
  idle_timeout_callback_.Reset(
      base::Bind(&mojo::Shell::Quit, base::Unretained(shell_)));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, idle_timeout_callback_.callback(),
      base::TimeDelta::FromSeconds(kIdleTimeoutInSeconds));
}

}  // namespace device
