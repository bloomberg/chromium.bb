// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_proxy.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace media {

namespace {

inline std::ostream& operator<<(std::ostream& out, CdmProxy::Status status) {
  switch (status) {
    case CdmProxy::Status::kOk:
      return out << "kOk";
    case CdmProxy::Status::kFail:
      return out << "kFail";
  }
  NOTREACHED();
  return out << "Invalid Status!";
}

cdm::CdmProxyClient::Status ToCdmStatus(CdmProxy::Status status) {
  switch (status) {
    case CdmProxy::Status::kOk:
      return cdm::CdmProxyClient::Status::kOk;
    case CdmProxy::Status::kFail:
      return cdm::CdmProxyClient::Status::kFail;
  }

  NOTREACHED() << "Unexpected status: " << status;
  return cdm::CdmProxyClient::Status::kFail;
}

cdm::CdmProxyClient::Protocol ToCdmProtocol(CdmProxy::Protocol protocol) {
  switch (protocol) {
    case CdmProxy::Protocol::kIntelConvergedSecurityAndManageabilityEngine:
      return cdm::CdmProxyClient::Protocol::
          kIntelConvergedSecurityAndManageabilityEngine;
  }

  // TODO(xhwang): Return an invalid protocol?
  NOTREACHED() << "Unexpected protocol: " << static_cast<int32_t>(protocol);
  return cdm::CdmProxyClient::Protocol::
      kIntelConvergedSecurityAndManageabilityEngine;
}

CdmProxy::Function ToMediaFunction(cdm::CdmProxy::Function function) {
  switch (function) {
    case cdm::CdmProxy::Function::kIntelNegotiateCryptoSessionKeyExchange:
      return CdmProxy::Function::kIntelNegotiateCryptoSessionKeyExchange;
  }

  // TODO(xhwang): Return an invalid function?
  NOTREACHED() << "Unexpected function: " << static_cast<int32_t>(function);
  return CdmProxy::Function::kIntelNegotiateCryptoSessionKeyExchange;
}

}  // namespace

MojoCdmProxy::MojoCdmProxy(Delegate* delegate, mojom::CdmProxyPtr cdm_proxy_ptr)
    : delegate_(delegate),
      cdm_proxy_ptr_(std::move(cdm_proxy_ptr)),
      client_binding_(this),
      weak_factory_(this) {
  DVLOG(1) << __func__;
  DCHECK(delegate_);
}

MojoCdmProxy::~MojoCdmProxy() {
  DVLOG(1) << __func__;
}

void MojoCdmProxy::Initialize(cdm::CdmProxyClient* client) {
  DVLOG(2) << __func__;

  DCHECK(client);
  client_ = client;

  mojom::CdmProxyClientAssociatedPtrInfo client_ptr_info;
  client_binding_.Bind(mojo::MakeRequest(&client_ptr_info));

  auto callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&MojoCdmProxy::OnInitialized, weak_factory_.GetWeakPtr()),
      media::CdmProxy::Status::kFail,
      media::CdmProxy::Protocol::kIntelConvergedSecurityAndManageabilityEngine,
      0);
  cdm_proxy_ptr_->Initialize(std::move(client_ptr_info), std::move(callback));
}

void MojoCdmProxy::Process(Function function,
                           uint32_t crypto_session_id,
                           const uint8_t* input_data,
                           uint32_t input_data_size,
                           uint32_t expected_output_data_size) {
  DVLOG(3) << __func__;
  CHECK(client_) << "Initialize not called.";

  auto callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&MojoCdmProxy::OnProcessed, weak_factory_.GetWeakPtr()),
      media::CdmProxy::Status::kFail, std::vector<uint8_t>());

  cdm_proxy_ptr_->Process(
      ToMediaFunction(function), crypto_session_id,
      std::vector<uint8_t>(input_data, input_data + input_data_size),
      expected_output_data_size, std::move(callback));
}

void MojoCdmProxy::CreateMediaCryptoSession(const uint8_t* input_data,
                                            uint32_t input_data_size) {
  DVLOG(3) << __func__;
  CHECK(client_) << "Initialize not called.";

  auto callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&MojoCdmProxy::OnMediaCryptoSessionCreated,
                     weak_factory_.GetWeakPtr()),
      media::CdmProxy::Status::kFail, 0, 0);

  cdm_proxy_ptr_->CreateMediaCryptoSession(
      std::vector<uint8_t>(input_data, input_data + input_data_size),
      std::move(callback));
}

void MojoCdmProxy::SetKey(uint32_t crypto_session_id,
                          const uint8_t* key_id,
                          uint32_t key_id_size,
                          const uint8_t* key_blob,
                          uint32_t key_blob_size) {
  DVLOG(3) << __func__;
  CHECK(client_) << "Initialize not called.";

  cdm_proxy_ptr_->SetKey(
      crypto_session_id, std::vector<uint8_t>(key_id, key_id + key_id_size),
      std::vector<uint8_t>(key_blob, key_blob + key_blob_size));
}

void MojoCdmProxy::RemoveKey(uint32_t crypto_session_id,
                             const uint8_t* key_id,
                             uint32_t key_id_size) {
  DVLOG(3) << __func__;
  CHECK(client_) << "Initialize not called.";

  cdm_proxy_ptr_->RemoveKey(crypto_session_id,
                            std::vector<uint8_t>(key_id, key_id + key_id_size));
}

void MojoCdmProxy::Destroy() {
  DVLOG(3) << __func__;

  // Note: |this| could be deleted as part of this call.
  delegate_->DestroyCdmProxy(this);
}

void MojoCdmProxy::NotifyHardwareReset() {
  DVLOG(2) << __func__;
  client_->NotifyHardwareReset();
}

void MojoCdmProxy::OnInitialized(media::CdmProxy::Status status,
                                 media::CdmProxy::Protocol protocol,
                                 uint32_t crypto_session_id) {
  DVLOG(3) << __func__ << ": status = " << status
           << ", crypto_session_id = " << crypto_session_id;
  client_->OnInitialized(ToCdmStatus(status), ToCdmProtocol(protocol),
                         crypto_session_id);
}

void MojoCdmProxy::OnProcessed(media::CdmProxy::Status status,
                               const std::vector<uint8_t>& output_data) {
  DVLOG(3) << __func__ << ": status = " << status;
  client_->OnProcessed(ToCdmStatus(status), output_data.data(),
                       output_data.size());
}

void MojoCdmProxy::OnMediaCryptoSessionCreated(media::CdmProxy::Status status,
                                               uint32_t crypto_session_id,
                                               uint64_t output_data) {
  DVLOG(3) << __func__ << ": status = " << status
           << ", crypto_session_id = " << crypto_session_id;
  client_->OnMediaCryptoSessionCreated(ToCdmStatus(status), crypto_session_id,
                                       output_data);
}

}  // namespace media
