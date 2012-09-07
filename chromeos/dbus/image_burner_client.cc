// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/image_burner_client.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The ImageBurnerClient implementation.
class ImageBurnerClientImpl : public ImageBurnerClient {
 public:
  explicit ImageBurnerClientImpl(dbus::Bus* bus)
      : proxy_(NULL),
        weak_ptr_factory_(this) {
    proxy_ = bus->GetObjectProxy(
        imageburn::kImageBurnServiceName,
        dbus::ObjectPath(imageburn::kImageBurnServicePath));
    proxy_->ConnectToSignal(
        imageburn::kImageBurnServiceInterface,
        imageburn::kSignalBurnFinishedName,
        base::Bind(&ImageBurnerClientImpl::OnBurnFinished,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&ImageBurnerClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        imageburn::kImageBurnServiceInterface,
        imageburn::kSignalBurnUpdateName,
        base::Bind(&ImageBurnerClientImpl::OnBurnProgressUpdate,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&ImageBurnerClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }
  virtual ~ImageBurnerClientImpl() {}

  // ImageBurnerClient override.
  virtual void BurnImage(const std::string& from_path,
                         const std::string& to_path,
                         const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(imageburn::kImageBurnServiceInterface,
                                 imageburn::kBurnImage);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(from_path);
    writer.AppendString(to_path);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&ImageBurnerClientImpl::OnBurnImage,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  error_callback));
  }

  // ImageBurnerClient override.
  virtual void SetEventHandlers(
      const BurnFinishedHandler& burn_finished_handler,
      const BurnProgressUpdateHandler& burn_progress_update_handler) OVERRIDE {
    burn_finished_handler_ = burn_finished_handler;
    burn_progress_update_handler_ = burn_progress_update_handler;
  }

  // ImageBurnerClient override.
  virtual void ResetEventHandlers() OVERRIDE {
    burn_finished_handler_.Reset();
    burn_progress_update_handler_.Reset();
  }

 private:
  // Called when a response for BurnImage is received
  void OnBurnImage(ErrorCallback error_callback, dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }
  }

  // Handles burn_finished signal and calls |handler|.
  void OnBurnFinished(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string target_path;
    bool success;
    std::string error;
    if (!reader.PopString(&target_path) ||
        !reader.PopBool(&success) ||
        !reader.PopString(&error)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (!burn_finished_handler_.is_null())
      burn_finished_handler_.Run(target_path, success, error);
  }

  // Handles burn_progress_udpate signal and calls |handler|.
  void OnBurnProgressUpdate(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string target_path;
    int64 num_bytes_burnt;
    int64 total_size;
    if (!reader.PopString(&target_path) ||
        !reader.PopInt64(&num_bytes_burnt) ||
        !reader.PopInt64(&total_size)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (!burn_progress_update_handler_.is_null())
      burn_progress_update_handler_.Run(target_path, num_bytes_burnt,
                                        total_size);
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool successed) {
    LOG_IF(ERROR, !successed) << "Connect to " << interface << " " <<
        signal << " failed.";
  }

  dbus::ObjectProxy* proxy_;
  BurnFinishedHandler burn_finished_handler_;
  BurnProgressUpdateHandler burn_progress_update_handler_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ImageBurnerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageBurnerClientImpl);
};

// A stub implementaion of ImageBurnerClient.
class ImageBurnerClientStubImpl : public ImageBurnerClient {
 public:
  ImageBurnerClientStubImpl() {}
  virtual ~ImageBurnerClientStubImpl() {}
  virtual void BurnImage(const std::string& from_path,
                         const std::string& to_path,
                         const ErrorCallback& error_callback) OVERRIDE {}
  virtual void SetEventHandlers(
      const BurnFinishedHandler& burn_finished_handler,
      const BurnProgressUpdateHandler& burn_progress_update_handler) OVERRIDE {}
  virtual void ResetEventHandlers() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageBurnerClientStubImpl);
};

}  // namespace

ImageBurnerClient::ImageBurnerClient() {
}

ImageBurnerClient::~ImageBurnerClient() {
}

// static
ImageBurnerClient* ImageBurnerClient::Create(DBusClientImplementationType type,
                                             dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new ImageBurnerClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new ImageBurnerClientStubImpl();
}

}  // chromeos
