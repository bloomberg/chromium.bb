// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/image_loader_client.h"

#include "base/bind.h"
#include "base/macros.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class ImageLoaderClientImpl : public ImageLoaderClient {
 public:
  ImageLoaderClientImpl() {}

  ~ImageLoaderClientImpl() override {}

  void RegisterComponent(const std::string& name,
                         const std::string& version,
                         const std::string& component_folder_abs_path,
                         const BoolDBusMethodCallback& callback) override {
    dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                                 imageloader::kRegisterComponent);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendString(version);
    writer.AppendString(component_folder_abs_path);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&ImageLoaderClientImpl::OnBoolMethod, callback));
  }

 protected:
  // DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        imageloader::kImageLoaderServiceName,
        dbus::ObjectPath(imageloader::kImageLoaderServicePath));
  }

 private:
  static void OnBoolMethod(const BoolDBusMethodCallback& callback,
                           dbus::Response* response) {
    if (!response) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, false);
      return;
    }
    dbus::MessageReader reader(response);
    bool result = false;
    if (!reader.PopBool(&result)) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, false);
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    callback.Run(DBUS_METHOD_CALL_SUCCESS, result);
  }

  dbus::ObjectProxy* proxy_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ImageLoaderClientImpl);
};

}  // namespace

ImageLoaderClient::ImageLoaderClient() {}

ImageLoaderClient::~ImageLoaderClient() {}

// static
ImageLoaderClient* ImageLoaderClient::Create() {
  return new ImageLoaderClientImpl();
}

}  // namespace chromeos
