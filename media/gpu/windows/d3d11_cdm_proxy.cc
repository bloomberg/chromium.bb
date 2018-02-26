// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/787657): Handle hardware key reset and notify the client.
#include "media/gpu/windows/d3d11_cdm_proxy.h"

#include <initguid.h>

#include "base/bind.h"
#include "base/logging.h"

namespace media {

D3D11CdmProxy::KeyInfo::KeyInfo() = default;

D3D11CdmProxy::KeyInfo::KeyInfo(uint32_t crypto_session_id,
                                std::vector<uint8_t> key_id,
                                std::vector<uint8_t> key_blob)
    : crypto_session_id(crypto_session_id),
      key_id(std::move(key_id)),
      key_blob(std::move(key_blob)) {}

D3D11CdmProxy::KeyInfo::KeyInfo(const KeyInfo&) = default;

D3D11CdmProxy::KeyInfo::~KeyInfo() {}

D3D11CdmProxy::D3D11CdmProxy(const GUID& stream_id,
                             CdmProxy::Protocol protocol,
                             const FunctionIdMap& function_id_map)
    : stream_id_(stream_id),
      protocol_(protocol),
      function_id_map_(function_id_map),
      create_device_func_(base::BindRepeating(D3D11CreateDevice)) {}

D3D11CdmProxy::~D3D11CdmProxy() {}

base::WeakPtr<CdmContext> D3D11CdmProxy::GetCdmContext() {
  // TODO(rkuroiwa): Implement CdmContext that returns the decrypt context
  // thru GetDecryptContext().
  return nullptr;
}

void D3D11CdmProxy::Initialize(Client* client, InitializeCB init_cb) {
  auto failed = [this, &init_cb]() {
    // The value doesn't matter as it shouldn't be used on a failure.
    const uint32_t kFailedCryptoSessionId = 0xFF;
    std::move(init_cb).Run(Status::kFail, protocol_, kFailedCryptoSessionId);
  };
  if (initialized_) {
    failed();
    NOTREACHED() << "CdmProxy should not be initialized more than once.";
    return;
  }

  client_ = client;

  D3D_FEATURE_LEVEL actual_feature_level = {};
  D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1,
                                        D3D_FEATURE_LEVEL_11_0};
  HRESULT hresult = create_device_func_.Run(
      nullptr,                            // No adapter.
      D3D_DRIVER_TYPE_HARDWARE, nullptr,  // No software rasterizer.
      0,                                  // flags, none.
      feature_levels, arraysize(feature_levels), D3D11_SDK_VERSION,
      device_.GetAddressOf(), &actual_feature_level,
      device_context_.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to create the D3D11Device:" << hresult;
    failed();
    return;
  }
  if (actual_feature_level != D3D_FEATURE_LEVEL_11_1) {
    DLOG(ERROR) << "D3D11 feature level too low: " << actual_feature_level;
    failed();
    return;
  }

  hresult = device_.CopyTo(video_device_.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to get ID3D11VideoDevice: " << hresult;
    failed();
    return;
  }

  hresult = device_context_.CopyTo(video_context_.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to get ID3D11VideoContext: " << hresult;
    failed();
    return;
  }

  hresult = device_.CopyTo(video_device1_.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to get ID3D11VideoDevice1: " << hresult;
    failed();
    return;
  }

  hresult = device_context_.CopyTo(video_context1_.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to get ID3D11VideoContext1: " << hresult;
    failed();
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11CryptoSession> csme_crypto_session;
  hresult = video_device_->CreateCryptoSession(
      &stream_id_, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
      &D3D11_KEY_EXCHANGE_HW_PROTECTION, csme_crypto_session.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to Create CryptoSession: " << hresult;
    failed();
    return;
  }

  hresult = video_device1_->GetCryptoSessionPrivateDataSize(
      &stream_id_, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
      &D3D11_KEY_EXCHANGE_HW_PROTECTION, &private_input_size_,
      &private_output_size_);
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to get private data sizes: " << hresult;
    failed();
    return;
  }

  const uint32_t crypto_session_id = next_crypto_session_id_++;
  crypto_session_map_[crypto_session_id] = std::move(csme_crypto_session);
  initialized_ = true;
  std::move(init_cb).Run(Status::kOk, protocol_, crypto_session_id);
}

void D3D11CdmProxy::Process(Function function,
                            uint32_t crypto_session_id,
                            const std::vector<uint8_t>& input_data_vec,
                            uint32_t expected_output_data_size,
                            ProcessCB process_cb) {
  auto failed = [&process_cb]() {
    std::move(process_cb).Run(Status::kFail, std::vector<uint8_t>());
  };

  if (!initialized_) {
    DLOG(ERROR) << "Not initialied.";
    failed();
    return;
  }

  auto function_id_it = function_id_map_.find(function);
  if (function_id_it == function_id_map_.end()) {
    DLOG(ERROR) << "Unrecognized function: " << static_cast<int>(function);
    failed();
    return;
  }

  auto crypto_session_it = crypto_session_map_.find(crypto_session_id);
  if (crypto_session_it == crypto_session_map_.end()) {
    DLOG(ERROR) << "Cannot find crypto session with ID " << crypto_session_id;
    failed();
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11CryptoSession>& crypto_session =
      crypto_session_it->second;

  D3D11_KEY_EXCHANGE_HW_PROTECTION_DATA key_exchange_data = {};
  key_exchange_data.HWProtectionFunctionID = function_id_it->second;

  // Because D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA and
  // D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA are variable size structures,
  // uint8 array are allocated and casted to each type.
  // -4 for the "BYTE pbInput[4]" field.
  std::unique_ptr<uint8_t[]> input_data_raw(
      new uint8_t[sizeof(D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA) - 4 +
                  input_data_vec.size()]);
  std::unique_ptr<uint8_t[]> output_data_raw(
      new uint8_t[sizeof(D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA) - 4 +
                  expected_output_data_size]);

  D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA* input_data =
      reinterpret_cast<D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA*>(
          input_data_raw.get());
  D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA* output_data =
      reinterpret_cast<D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA*>(
          output_data_raw.get());

  key_exchange_data.pInputData = input_data;
  key_exchange_data.pOutputData = output_data;
  input_data->PrivateDataSize = private_input_size_;
  input_data->HWProtectionDataSize = 0;
  memcpy(input_data->pbInput, input_data_vec.data(), input_data_vec.size());

  output_data->PrivateDataSize = private_output_size_;
  output_data->HWProtectionDataSize = 0;
  output_data->TransportTime = 0;
  output_data->ExecutionTime = 0;
  output_data->MaxHWProtectionDataSize = expected_output_data_size;

  HRESULT hresult = video_context_->NegotiateCryptoSessionKeyExchange(
      crypto_session.Get(), sizeof(key_exchange_data), &key_exchange_data);
  if (FAILED(hresult)) {
    failed();
    return;
  }

  std::vector<uint8_t> output_vec;
  output_vec.reserve(expected_output_data_size);
  memcpy(output_vec.data(), output_data->pbOutput, expected_output_data_size);
  std::move(process_cb).Run(Status::kOk, output_vec);
  return;
}

void D3D11CdmProxy::CreateMediaCryptoSession(
    const std::vector<uint8_t>& input_data,
    CreateMediaCryptoSessionCB create_media_crypto_session_cb) {
  auto failed = [&create_media_crypto_session_cb]() {
    const uint32_t kInvalidSessionId = 0;
    const uint64_t kNoOutputData = 0;
    std::move(create_media_crypto_session_cb)
        .Run(Status::kFail, kInvalidSessionId, kNoOutputData);
  };
  if (!initialized_) {
    DLOG(ERROR) << "Not initialized.";
    failed();
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11CryptoSession> media_crypto_session;
  HRESULT hresult = video_device_->CreateCryptoSession(
      &stream_id_, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT, &stream_id_,
      media_crypto_session.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to create a crypto session: " << hresult;
    failed();
    return;
  }

  // Don't do CheckCryptoSessionStatus() yet. The status may be something like
  // CONTEXT_LOST because GetDataForNewHardwareKey() is not called yet.
  uint64_t output_data = 0;
  if (!input_data.empty()) {
    hresult = video_context1_->GetDataForNewHardwareKey(
        media_crypto_session.Get(), input_data.size(), input_data.data(),
        &output_data);
    if (FAILED(hresult)) {
      DLOG(ERROR) << "Failed to establish hardware session: " << hresult;
      failed();
      return;
    }
  }

  D3D11_CRYPTO_SESSION_STATUS crypto_session_status = {};
  hresult = video_context1_->CheckCryptoSessionStatus(
      media_crypto_session.Get(), &crypto_session_status);
  if (FAILED(hresult) ||
      crypto_session_status != D3D11_CRYPTO_SESSION_STATUS_OK) {
    DLOG(ERROR) << "Crypto session is not OK. Crypto session status "
                << crypto_session_status << ". HRESULT " << hresult;
    failed();
    return;
  }

  const uint32_t media_crypto_session_id = next_crypto_session_id_++;
  crypto_session_map_[media_crypto_session_id] =
      std::move(media_crypto_session);
  std::move(create_media_crypto_session_cb)
      .Run(Status::kOk, media_crypto_session_id, output_data);
}

void D3D11CdmProxy::SetKey(uint32_t crypto_session_id,
                           const std::vector<uint8_t>& key_id,
                           const std::vector<uint8_t>& key_blob) {
  KeyInfo key_info(crypto_session_id, key_id, key_blob);
  // Note that this would overwrite an entry but it is completely valid, e.g.
  // updating the keyblob due to a configuration change.
  key_info_map_[key_id] = std::move(key_info);
}

void D3D11CdmProxy::RemoveKey(uint32_t crypto_session_id,
                              const std::vector<uint8_t>& key_id) {
  key_info_map_.erase(key_id);
}

void D3D11CdmProxy::SetCreateDeviceCallbackForTesting(CreateDeviceCB callback) {
  create_device_func_ = std::move(callback);
}

}  // namespace media
