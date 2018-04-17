// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/787657): Handle hardware key reset and notify the client.
#include "media/gpu/windows/d3d11_cdm_proxy.h"

#include <initguid.h>

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_proxy_context.h"

namespace media {

namespace {

class D3D11CdmProxyContext : public CdmProxyContext {
 public:
  explicit D3D11CdmProxyContext(const GUID& key_info_guid)
      : key_info_guid_(key_info_guid) {}
  ~D3D11CdmProxyContext() override = default;

  // The pointers are owned by the caller.
  void SetKey(ID3D11CryptoSession* crypto_session,
              const std::vector<uint8_t>& key_id,
              const std::vector<uint8_t>& key_blob) {
    std::string key_id_str(key_id.begin(), key_id.end());
    KeyInfo key_info(crypto_session, key_blob);
    // Note that this would overwrite an entry but it is completely valid, e.g.
    // updating the keyblob due to a configuration change.
    key_info_map_[key_id_str] = std::move(key_info);
  }

  void RemoveKey(ID3D11CryptoSession* crypto_session,
                 const std::vector<uint8_t>& key_id) {
    std::string key_id_str(key_id.begin(), key_id.end());
    key_info_map_.erase(key_id_str);
  }

  // CdmProxyContext implementation.
  base::Optional<D3D11DecryptContext> GetD3D11DecryptContext(
      const std::string& key_id) override {
    auto key_info_it = key_info_map_.find(key_id);
    if (key_info_it == key_info_map_.end())
      return base::nullopt;

    auto& key_info = key_info_it->second;
    D3D11DecryptContext context = {};
    context.crypto_session = key_info.crypto_session;
    context.key_blob = key_info.key_blob.data();
    context.key_blob_size = key_info.key_blob.size();
    context.key_info_guid = key_info_guid_;
    return context;
  }

 private:
  // A structure to keep the data passed to SetKey(). See documentation for
  // SetKey() for what the fields mean.
  struct KeyInfo {
    KeyInfo() = default;
    KeyInfo(ID3D11CryptoSession* crypto_session, std::vector<uint8_t> key_blob)
        : crypto_session(crypto_session), key_blob(std::move(key_blob)) {}
    KeyInfo(const KeyInfo&) = default;
    ~KeyInfo() = default;
    ID3D11CryptoSession* crypto_session;
    std::vector<uint8_t> key_blob;
  };

  // Maps key ID to KeyInfo.
  // The key ID's type is string, which is converted from |key_id| in
  // SetKey(). It's better to use string here rather than convert
  // vector<uint8_t> to string every time in GetD3D11DecryptContext() because
  // in most cases it would be called more often than SetKey() and RemoveKey()
  // combined.
  std::map<std::string, KeyInfo> key_info_map_;

  const GUID key_info_guid_;

  DISALLOW_COPY_AND_ASSIGN(D3D11CdmProxyContext);
};

}  // namespace

class D3D11CdmContext : public CdmContext {
 public:
  explicit D3D11CdmContext(const GUID& key_info_guid)
      : cdm_proxy_context_(key_info_guid), weak_factory_(this) {}
  ~D3D11CdmContext() override = default;

  // The pointers are owned by the caller.
  void SetKey(ID3D11CryptoSession* crypto_session,
              const std::vector<uint8_t>& key_id,
              const std::vector<uint8_t>& key_blob) {
    cdm_proxy_context_.SetKey(crypto_session, key_id, key_blob);
  }
  void RemoveKey(ID3D11CryptoSession* crypto_session,
                 const std::vector<uint8_t>& key_id) {
    cdm_proxy_context_.RemoveKey(crypto_session, key_id);
  }

  base::WeakPtr<D3D11CdmContext> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // CdmContext implementation.
  std::unique_ptr<CallbackRegistration> RegisterNewKeyCB(
      base::RepeatingClosure new_key_cb) override {
    return new_key_callbacks_.Register(std::move(new_key_cb));
  }
  CdmProxyContext* GetCdmProxyContext() override { return &cdm_proxy_context_; }

 private:
  D3D11CdmProxyContext cdm_proxy_context_;

  // TODO(rkuroiwa): Call Notify() when new usable key is available.
  ClosureRegistry new_key_callbacks_;

  base::WeakPtrFactory<D3D11CdmContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(D3D11CdmContext);
};

D3D11CdmProxy::D3D11CdmProxy(const GUID& crypto_type,
                             CdmProxy::Protocol protocol,
                             const FunctionIdMap& function_id_map)
    : crypto_type_(crypto_type),
      protocol_(protocol),
      function_id_map_(function_id_map),
      cdm_context_(std::make_unique<D3D11CdmContext>(crypto_type)),
      create_device_func_(base::BindRepeating(D3D11CreateDevice)) {}

D3D11CdmProxy::~D3D11CdmProxy() {}

base::WeakPtr<CdmContext> D3D11CdmProxy::GetCdmContext() {
  return cdm_context_->GetWeakPtr();
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

  const D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1};

  HRESULT hresult = create_device_func_.Run(
      nullptr,                            // No adapter.
      D3D_DRIVER_TYPE_HARDWARE, nullptr,  // No software rasterizer.
      0,                                  // flags, none.
      feature_levels, arraysize(feature_levels), D3D11_SDK_VERSION,
      device_.GetAddressOf(), nullptr, device_context_.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to create the D3D11Device:" << hresult;
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
      &crypto_type_, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
      &D3D11_KEY_EXCHANGE_HW_PROTECTION, csme_crypto_session.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to Create CryptoSession: " << hresult;
    failed();
    return;
  }

  hresult = video_device1_->GetCryptoSessionPrivateDataSize(
      &crypto_type_, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
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
      &crypto_type_, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT, &crypto_type_,
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
  auto crypto_session_it = crypto_session_map_.find(crypto_session_id);
  if (crypto_session_it == crypto_session_map_.end()) {
    DLOG(WARNING) << crypto_session_id
                  << " did not map to a crypto session instance.";
    return;
  }
  cdm_context_->SetKey(crypto_session_it->second.Get(), key_id, key_blob);
}

void D3D11CdmProxy::RemoveKey(uint32_t crypto_session_id,
                              const std::vector<uint8_t>& key_id) {
  auto crypto_session_it = crypto_session_map_.find(crypto_session_id);
  if (crypto_session_it == crypto_session_map_.end()) {
    DLOG(WARNING) << crypto_session_id
                  << " did not map to a crypto session instance.";
    return;
  }
  cdm_context_->RemoveKey(crypto_session_it->second.Get(), key_id);
}

void D3D11CdmProxy::SetCreateDeviceCallbackForTesting(CreateDeviceCB callback) {
  create_device_func_ = std::move(callback);
}

}  // namespace media
