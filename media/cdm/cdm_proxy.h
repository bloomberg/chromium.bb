// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_PROXY_H_
#define MEDIA_CDM_CDM_PROXY_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "media/base/media_export.h"

namespace media {

// Key information structure containing data necessary to decrypt/decode media.
struct MEDIA_EXPORT KeyInfo {
  KeyInfo();
  ~KeyInfo();
  // Crypto session for decryption.
  uint32_t crypto_session_id = 0;
  // ID of the key.
  std::vector<uint8_t> key_id;
  // Opaque key blob for decrypting or decoding.
  std::vector<uint8_t> key_blob;
  // Indicates whether this key/key_id is usable. The caller sets this to false
  // to invalidate a key.
  bool is_usable_key = true;
};

// A proxy for the CDM.
// In general, the interpretation of the method and callback parameters are
// protocol dependent. For enum parameters, values outside the enum range may
// not work.
class MEDIA_EXPORT CdmProxy {
 public:
  // Client of the proxy.
  class Client {
   public:
    Client() {}
    virtual ~Client() {}
    // Called when there is a hardware reset and all the hardware context is
    // lost.
    virtual void NotifyHardwareReset() = 0;
  };

  enum class Status {
    kOk,
    kFail,
  };

  enum class Protocol {
    // Method using Intel CSME.
    kIntelConvergedSecurityAndManageabilityEngine,
    // There will be more values in the future e.g. kD3D11RsaHardware,
    // kD3D11RsaSoftware to use the D3D11 RSA method.
  };

  enum class Function {
    // For Intel CSME path to call
    // ID3D11VideoContext::NegotiateCryptoSessionKeyExchange.
    kIntelNegotiateCryptoSessionKeyExchange,
    // There will be more values in the future e.g. for D3D11 RSA method.
  };

  CdmProxy() {}
  virtual ~CdmProxy() {}

  // Callback for Initialize(). If the proxy created a crypto session, then the
  // ID for the crypto session is |crypto_session_id|.
  using InitializeCB = base::OnceCallback<
      void(Status status, Protocol protocol, uint32_t crypto_session_id)>;

  // Initializes the proxy. The status and the return values of the call is
  // reported to |init_cb|.
  virtual void Initialize(Client* client, InitializeCB init_cb) = 0;

  // Callback for Process(). |output_data| is the output of processing.
  using ProcessCB =
      base::OnceCallback<void(Status status,
                              const std::vector<uint8_t>& output_data)>;

  // Processes and updates the state of the proxy.
  // |expected_output_size| is the size of the output data passed to the
  // callback. Whether this value is required or not is protocol dependent.
  // The status and the return values of the call is reported to |process_cb|.
  virtual void Process(Function function,
                       uint32_t crypto_session_id,
                       const std::vector<uint8_t>& input_data,
                       uint32_t expected_output_data_size,
                       ProcessCB process_cb) = 0;

  // Callback for CreateMediaCryptoSession().
  // On suceess:
  // |crypto_session_id| is the ID for the created crypto session.
  // |output_data| is extra value, if any.
  using CreateMediaCryptoSessionCB = base::OnceCallback<
      void(Status status, uint32_t crypto_session_id, uint64_t output_data)>;

  // Creates a crypto session for handling media.
  // If extra data has to be passed to further setup the media crypto session,
  // pass the data as |input_data|.
  // The status and the return values of the call is reported to
  // |create_media_crypto_session_cb|.
  virtual void CreateMediaCryptoSession(
      const std::vector<uint8_t>& input_data,
      CreateMediaCryptoSessionCB create_media_crypto_session_cb) = 0;

  // Send multiple key information to the proxy.
  virtual void SetKeyInfo(const std::vector<KeyInfo>& key_infos) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmProxy);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_PROXY_H_