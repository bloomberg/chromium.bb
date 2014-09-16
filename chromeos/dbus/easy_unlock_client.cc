// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/easy_unlock_client.h"

#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Reads array of bytes from a dbus message reader and converts it to string.
std::string PopResponseData(dbus::MessageReader* reader) {
  const uint8* bytes = NULL;
  size_t length = 0;
  if (!reader->PopArrayOfBytes(&bytes, &length))
    return "";

  return std::string(reinterpret_cast<const char*>(bytes), length);
}

// Converts string to array of bytes and writes it using dbus meddage writer.
void AppendStringAsByteArray(const std::string& data,
                             dbus::MessageWriter* writer) {
  writer->AppendArrayOfBytes(reinterpret_cast<const uint8*>(data.data()),
                             data.length());
}

// The EasyUnlockClient used in production (and returned by
// EasyUnlockClient::Create).
class EasyUnlockClientImpl : public EasyUnlockClient {
 public:
  EasyUnlockClientImpl() : proxy_(NULL), weak_ptr_factory_(this) {}

  virtual ~EasyUnlockClientImpl() {}

  // EasyUnlockClient override.
  virtual void PerformECDHKeyAgreement(const std::string& private_key,
                                       const std::string& public_key,
                                       const DataCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        easy_unlock::kEasyUnlockServiceInterface,
        easy_unlock::kPerformECDHKeyAgreementMethod);
    dbus::MessageWriter writer(&method_call);
    // NOTE: DBus expects that data sent as string is UTF-8 encoded. This is
    //     not guaranteed here, so the method uses byte arrays.
    AppendStringAsByteArray(private_key, &writer);
    AppendStringAsByteArray(public_key, &writer);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&EasyUnlockClientImpl::OnData,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // EasyUnlockClient override.
  virtual void GenerateEcP256KeyPair(const KeyPairCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        easy_unlock::kEasyUnlockServiceInterface,
        easy_unlock::kGenerateEcP256KeyPairMethod);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&EasyUnlockClientImpl::OnKeyPair,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // EasyUnlockClient override.
  virtual void CreateSecureMessage(const std::string& payload,
                                   const CreateSecureMessageOptions& options,
                                   const DataCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        easy_unlock::kEasyUnlockServiceInterface,
        easy_unlock::kCreateSecureMessageMethod);
    dbus::MessageWriter writer(&method_call);
    // NOTE: DBus expects that data sent as string is UTF-8 encoded. This is
    //     not guaranteed here, so the method uses byte arrays.
    AppendStringAsByteArray(payload, &writer);
    AppendStringAsByteArray(options.key, &writer);
    AppendStringAsByteArray(options.associated_data, &writer);
    AppendStringAsByteArray(options.public_metadata, &writer);
    AppendStringAsByteArray(options.verification_key_id, &writer);
    AppendStringAsByteArray(options.decryption_key_id, &writer);
    writer.AppendString(options.encryption_type);
    writer.AppendString(options.signature_type);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&EasyUnlockClientImpl::OnData,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // EasyUnlockClient override.
  virtual void UnwrapSecureMessage(const std::string& message,
                                   const UnwrapSecureMessageOptions& options,
                                   const DataCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        easy_unlock::kEasyUnlockServiceInterface,
        easy_unlock::kUnwrapSecureMessageMethod);
    dbus::MessageWriter writer(&method_call);
    // NOTE: DBus expects that data sent as string is UTF-8 encoded. This is
    //     not guaranteed here, so the method uses byte arrays.
    AppendStringAsByteArray(message, &writer);
    AppendStringAsByteArray(options.key, &writer);
    AppendStringAsByteArray(options.associated_data, &writer);
    writer.AppendString(options.encryption_type);
    writer.AppendString(options.signature_type);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&EasyUnlockClientImpl::OnData,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    proxy_ =
        bus->GetObjectProxy(
            easy_unlock::kEasyUnlockServiceName,
            dbus::ObjectPath(easy_unlock::kEasyUnlockServicePath));
  }

 private:
  void OnData(const DataCallback& callback, dbus::Response* response) {
    if (!response) {
      callback.Run("");
      return;
    }

    dbus::MessageReader reader(response);
    callback.Run(PopResponseData(&reader));
  }

  void OnKeyPair(const KeyPairCallback& callback, dbus::Response* response) {
    if (!response) {
      callback.Run("", "");
      return;
    }

    dbus::MessageReader reader(response);
    std::string private_key = PopResponseData(&reader);
    std::string public_key = PopResponseData(&reader);

    if (public_key.empty() || private_key.empty()) {
      callback.Run("", "");
      return;
    }

    callback.Run(private_key, public_key);
  }

  dbus::ObjectProxy* proxy_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EasyUnlockClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockClientImpl);
};

}  // namespace

EasyUnlockClient::CreateSecureMessageOptions::CreateSecureMessageOptions() {}

EasyUnlockClient::CreateSecureMessageOptions::~CreateSecureMessageOptions() {}

EasyUnlockClient::UnwrapSecureMessageOptions::UnwrapSecureMessageOptions() {}

EasyUnlockClient::UnwrapSecureMessageOptions::~UnwrapSecureMessageOptions() {}

EasyUnlockClient::EasyUnlockClient() {
}

EasyUnlockClient::~EasyUnlockClient() {
}

// static
EasyUnlockClient* EasyUnlockClient::Create() {
  return new EasyUnlockClientImpl();
}

}  // namespace chromeos
