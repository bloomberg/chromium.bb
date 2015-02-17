// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/encrypted_media_player_support.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/key_systems.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModule.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using blink::WebMediaPlayer;
using blink::WebMediaPlayerClient;
using blink::WebString;

namespace media {

#define BIND_TO_RENDER_LOOP(function)            \
  (BindToCurrentLoop(base::Bind(function, AsWeakPtr())))

#define BIND_TO_RENDER_LOOP1(function, arg1)     \
  (BindToCurrentLoop(base::Bind(function, AsWeakPtr(), arg1)))

// Prefix for histograms related to Encrypted Media Extensions.
static const char* kMediaEme = "Media.EME.";

// Convert a WebString to ASCII, falling back on an empty string in the case
// of a non-ASCII string.
static std::string ToASCIIOrEmpty(const WebString& string) {
  return base::IsStringASCII(string) ? base::UTF16ToASCII(string)
                                     : std::string();
}

// Helper functions to report media EME related stats to UMA. They follow the
// convention of more commonly used macros UMA_HISTOGRAM_ENUMERATION and
// UMA_HISTOGRAM_COUNTS. The reason that we cannot use those macros directly is
// that UMA_* macros require the names to be constant throughout the process'
// lifetime.
static void EmeUMAHistogramEnumeration(const std::string& key_system,
                                       const std::string& method,
                                       int sample,
                                       int boundary_value) {
  base::LinearHistogram::FactoryGet(
      kMediaEme + GetKeySystemNameForUMA(key_system) + "." + method,
      1, boundary_value, boundary_value + 1,
      base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

static void EmeUMAHistogramCounts(const std::string& key_system,
                                  const std::string& method,
                                  int sample) {
  // Use the same parameters as UMA_HISTOGRAM_COUNTS.
  base::Histogram::FactoryGet(
      kMediaEme + GetKeySystemNameForUMA(key_system) + "." + method,
      1, 1000000, 50, base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

// Helper enum for reporting generateKeyRequest/addKey histograms.
enum MediaKeyException {
  kUnknownResultId,
  kSuccess,
  kKeySystemNotSupported,
  kInvalidPlayerState,
  kMaxMediaKeyException
};

static MediaKeyException MediaKeyExceptionForUMA(
    WebMediaPlayer::MediaKeyException e) {
  switch (e) {
    case WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported:
      return kKeySystemNotSupported;
    case WebMediaPlayer::MediaKeyExceptionInvalidPlayerState:
      return kInvalidPlayerState;
    case WebMediaPlayer::MediaKeyExceptionNoError:
      return kSuccess;
    default:
      return kUnknownResultId;
  }
}

// Helper for converting |key_system| name and exception |e| to a pair of enum
// values from above, for reporting to UMA.
static void ReportMediaKeyExceptionToUMA(const std::string& method,
                                         const std::string& key_system,
                                         WebMediaPlayer::MediaKeyException e) {
  MediaKeyException result_id = MediaKeyExceptionForUMA(e);
  DCHECK_NE(result_id, kUnknownResultId) << e;
  EmeUMAHistogramEnumeration(
      key_system, method, result_id, kMaxMediaKeyException);
}

// Guess the type of |init_data|. This is only used to handle some corner cases
// so we keep it as simple as possible without breaking major use cases.
static std::string GuessInitDataType(const unsigned char* init_data,
                                     unsigned init_data_length) {
  // Most WebM files use KeyId of 16 bytes. CENC init data is always >16 bytes.
  if (init_data_length == 16)
    return "webm";

  return "cenc";
}

EncryptedMediaPlayerSupport::EncryptedMediaPlayerSupport(
    scoped_ptr<CdmFactory> cdm_factory,
    blink::WebMediaPlayerClient* client,
    MediaPermission* media_permission,
    const SetCdmContextCB& set_cdm_context_cb)
    : cdm_factory_(cdm_factory.Pass()),
      client_(client),
      media_permission_(media_permission),
      set_cdm_context_cb_(set_cdm_context_cb) {
}

EncryptedMediaPlayerSupport::~EncryptedMediaPlayerSupport() {
}

WebMediaPlayer::MediaKeyException
EncryptedMediaPlayerSupport::GenerateKeyRequest(
    blink::WebLocalFrame* frame,
    const WebString& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  DVLOG(1) << "generateKeyRequest: " << base::string16(key_system) << ": "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length));

  std::string ascii_key_system =
      GetUnprefixedKeySystemName(ToASCIIOrEmpty(key_system));

  WebMediaPlayer::MediaKeyException e = GenerateKeyRequestInternal(
      frame, ascii_key_system, init_data, init_data_length);
  ReportMediaKeyExceptionToUMA("generateKeyRequest", ascii_key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException
EncryptedMediaPlayerSupport::GenerateKeyRequestInternal(
    blink::WebLocalFrame* frame,
    const std::string& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  if (!PrefixedIsSupportedConcreteKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  // We do not support run-time switching between key systems for now.
  if (current_key_system_.empty()) {
    if (!proxy_decryptor_) {
      proxy_decryptor_.reset(new ProxyDecryptor(
          media_permission_,
          BIND_TO_RENDER_LOOP(&EncryptedMediaPlayerSupport::OnKeyAdded),
          BIND_TO_RENDER_LOOP(&EncryptedMediaPlayerSupport::OnKeyError),
          BIND_TO_RENDER_LOOP(&EncryptedMediaPlayerSupport::OnKeyMessage)));
    }

    GURL security_origin(frame->document().securityOrigin().toString());

    if (!proxy_decryptor_->InitializeCDM(cdm_factory_.get(), key_system,
                                         security_origin)) {
      return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
    }

    if (proxy_decryptor_ && !set_cdm_context_cb_.is_null()) {
      base::ResetAndReturn(&set_cdm_context_cb_)
          .Run(proxy_decryptor_->GetCdmContext(),
               base::Bind(&IgnoreCdmAttached));
    }

    current_key_system_ = key_system;
  } else if (key_system != current_key_system_) {
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;
  }

  std::string init_data_type = init_data_type_;
  if (init_data_type.empty())
    init_data_type = GuessInitDataType(init_data, init_data_length);

  if (!proxy_decryptor_->GenerateKeyRequest(init_data_type, init_data,
                                            init_data_length)) {
    current_key_system_.clear();
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
  }

  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException EncryptedMediaPlayerSupport::AddKey(
    const WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const WebString& session_id) {
  DVLOG(1) << "addKey: " << base::string16(key_system) << ": "
           << std::string(reinterpret_cast<const char*>(key),
                          static_cast<size_t>(key_length)) << ", "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length)) << " ["
           << base::string16(session_id) << "]";

  std::string ascii_key_system =
      GetUnprefixedKeySystemName(ToASCIIOrEmpty(key_system));
  std::string ascii_session_id = ToASCIIOrEmpty(session_id);

  WebMediaPlayer::MediaKeyException e = AddKeyInternal(ascii_key_system,
                                                       key,
                                                       key_length,
                                                       init_data,
                                                       init_data_length,
                                                       ascii_session_id);
  ReportMediaKeyExceptionToUMA("addKey", ascii_key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException
EncryptedMediaPlayerSupport::AddKeyInternal(
    const std::string& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const std::string& session_id) {
  DCHECK(key);
  DCHECK_GT(key_length, 0u);

  if (!PrefixedIsSupportedConcreteKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.empty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  proxy_decryptor_->AddKey(
      key, key_length, init_data, init_data_length, session_id);
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException
EncryptedMediaPlayerSupport::CancelKeyRequest(
    const WebString& key_system,
    const WebString& session_id) {
  DVLOG(1) << "cancelKeyRequest: " << base::string16(key_system) << ": "
           << " [" << base::string16(session_id) << "]";

  std::string ascii_key_system =
      GetUnprefixedKeySystemName(ToASCIIOrEmpty(key_system));
  std::string ascii_session_id = ToASCIIOrEmpty(session_id);

  WebMediaPlayer::MediaKeyException e =
      CancelKeyRequestInternal(ascii_key_system, ascii_session_id);
  ReportMediaKeyExceptionToUMA("cancelKeyRequest", ascii_key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException
EncryptedMediaPlayerSupport::CancelKeyRequestInternal(
    const std::string& key_system,
    const std::string& session_id) {
  if (!PrefixedIsSupportedConcreteKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.empty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  proxy_decryptor_->CancelKeyRequest(session_id);
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

void EncryptedMediaPlayerSupport::SetInitDataType(
    const std::string& init_data_type) {
  DCHECK(!init_data_type.empty());
  DLOG_IF(WARNING,
          !init_data_type_.empty() && init_data_type != init_data_type_)
      << "Mixed init data type not supported. The new type is ignored.";
  if (init_data_type_.empty())
    init_data_type_ = init_data_type;
}

void EncryptedMediaPlayerSupport::OnPipelineDecryptError() {
  EmeUMAHistogramCounts(current_key_system_, "DecryptError", 1);
}

void EncryptedMediaPlayerSupport::OnKeyAdded(const std::string& session_id) {
  EmeUMAHistogramCounts(current_key_system_, "KeyAdded", 1);
  client_->keyAdded(
      WebString::fromUTF8(GetPrefixedKeySystemName(current_key_system_)),
      WebString::fromUTF8(session_id));
}

void EncryptedMediaPlayerSupport::OnKeyError(const std::string& session_id,
                                             MediaKeys::KeyError error_code,
                                             uint32 system_code) {
  EmeUMAHistogramEnumeration(current_key_system_, "KeyError",
                             error_code, MediaKeys::kMaxKeyError);

  uint16 short_system_code = 0;
  if (system_code > std::numeric_limits<uint16>::max()) {
    LOG(WARNING) << "system_code exceeds unsigned short limit.";
    short_system_code = std::numeric_limits<uint16>::max();
  } else {
    short_system_code = static_cast<uint16>(system_code);
  }

  client_->keyError(
      WebString::fromUTF8(GetPrefixedKeySystemName(current_key_system_)),
      WebString::fromUTF8(session_id),
      static_cast<WebMediaPlayerClient::MediaKeyErrorCode>(error_code),
      short_system_code);
}

void EncryptedMediaPlayerSupport::OnKeyMessage(
    const std::string& session_id,
    const std::vector<uint8>& message,
    const GURL& destination_url) {
  DCHECK(destination_url.is_empty() || destination_url.is_valid());

  client_->keyMessage(
      WebString::fromUTF8(GetPrefixedKeySystemName(current_key_system_)),
      WebString::fromUTF8(session_id),
      message.empty() ? NULL : &message[0],
      base::saturated_cast<unsigned int>(message.size()),
      destination_url);
}

}  // namespace media
