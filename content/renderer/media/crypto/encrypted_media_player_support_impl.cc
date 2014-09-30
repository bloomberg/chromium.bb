// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/encrypted_media_player_support_impl.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "content/renderer/media/webcontentdecryptionmodule_impl.h"
#include "content/renderer/pepper/pepper_webplugin_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/blink/encrypted_media_player_support.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModule.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/pepper_cdm_wrapper_impl.h"
#endif

using blink::WebMediaPlayer;
using blink::WebMediaPlayerClient;
using blink::WebString;

namespace content {

#define BIND_TO_RENDER_LOOP(function)            \
  (media::BindToCurrentLoop(base::Bind(function, AsWeakPtr())))

#define BIND_TO_RENDER_LOOP1(function, arg1)     \
  (media::BindToCurrentLoop(base::Bind(function, AsWeakPtr(), arg1)))


// Prefix for histograms related to Encrypted Media Extensions.
static const char* kMediaEme = "Media.EME.";

// Used for calls to decryptor_ready_cb where the result can be ignored.
static void DoNothing(bool success) {
}

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
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method,
      1, boundary_value, boundary_value + 1,
      base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

static void EmeUMAHistogramCounts(const std::string& key_system,
                                  const std::string& method,
                                  int sample) {
  // Use the same parameters as UMA_HISTOGRAM_COUNTS.
  base::Histogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method,
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
  // Most WebM files use KeyId of 16 bytes. MP4 init data are always >16 bytes.
  if (init_data_length == 16)
    return "video/webm";

  return "video/mp4";
}

scoped_ptr<media::EncryptedMediaPlayerSupport>
EncryptedMediaPlayerSupportImpl::Create(blink::WebMediaPlayerClient* client) {
  return scoped_ptr<EncryptedMediaPlayerSupport>(
      new EncryptedMediaPlayerSupportImpl(client));
}

EncryptedMediaPlayerSupportImpl::EncryptedMediaPlayerSupportImpl(
    blink::WebMediaPlayerClient* client)
    : client_(client),
      web_cdm_(NULL) {
}

EncryptedMediaPlayerSupportImpl::~EncryptedMediaPlayerSupportImpl() {
}

WebMediaPlayer::MediaKeyException
EncryptedMediaPlayerSupportImpl::GenerateKeyRequest(
    blink::WebLocalFrame* frame,
    const WebString& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  DVLOG(1) << "generateKeyRequest: " << base::string16(key_system) << ": "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length));

  std::string ascii_key_system =
      GetUnprefixedKeySystemName(ToASCIIOrEmpty(key_system));

  WebMediaPlayer::MediaKeyException e =
      GenerateKeyRequestInternal(frame, ascii_key_system, init_data,
                                 init_data_length);
  ReportMediaKeyExceptionToUMA("generateKeyRequest", ascii_key_system, e);
  return e;
}


WebMediaPlayer::MediaKeyException
EncryptedMediaPlayerSupportImpl::GenerateKeyRequestInternal(
    blink::WebLocalFrame* frame,
    const std::string& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  if (!IsConcreteSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  // We do not support run-time switching between key systems for now.
  if (current_key_system_.empty()) {
    if (!proxy_decryptor_) {
      proxy_decryptor_.reset(new ProxyDecryptor(
#if defined(ENABLE_PEPPER_CDMS)
          // Create() must be called synchronously as |frame| may not be
          // valid afterwards.
          base::Bind(&PepperCdmWrapperImpl::Create, frame),
#elif defined(ENABLE_BROWSER_CDMS)
#error Browser side CDM in WMPI for prefixed EME API not supported yet.
#endif
          BIND_TO_RENDER_LOOP(&EncryptedMediaPlayerSupportImpl::OnKeyAdded),
          BIND_TO_RENDER_LOOP(&EncryptedMediaPlayerSupportImpl::OnKeyError),
          BIND_TO_RENDER_LOOP(&EncryptedMediaPlayerSupportImpl::OnKeyMessage)));
    }

    GURL security_origin(frame->document().securityOrigin().toString());
    if (!proxy_decryptor_->InitializeCDM(key_system, security_origin))
      return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

    if (proxy_decryptor_ && !decryptor_ready_cb_.is_null()) {
      base::ResetAndReturn(&decryptor_ready_cb_)
          .Run(proxy_decryptor_->GetDecryptor(), base::Bind(DoNothing));
    }

    current_key_system_ = key_system;
  } else if (key_system != current_key_system_) {
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;
  }

  std::string init_data_type = init_data_type_;
  if (init_data_type.empty())
    init_data_type = GuessInitDataType(init_data, init_data_length);

  // TODO(xhwang): We assume all streams are from the same container (thus have
  // the same "type") for now. In the future, the "type" should be passed down
  // from the application.
  if (!proxy_decryptor_->GenerateKeyRequest(
           init_data_type, init_data, init_data_length)) {
    current_key_system_.clear();
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
  }

  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException EncryptedMediaPlayerSupportImpl::AddKey(
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
EncryptedMediaPlayerSupportImpl::AddKeyInternal(
    const std::string& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const std::string& session_id) {
  DCHECK(key);
  DCHECK_GT(key_length, 0u);

  if (!IsConcreteSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.empty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  proxy_decryptor_->AddKey(
      key, key_length, init_data, init_data_length, session_id);
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException
EncryptedMediaPlayerSupportImpl::CancelKeyRequest(
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
EncryptedMediaPlayerSupportImpl::CancelKeyRequestInternal(
    const std::string& key_system,
    const std::string& session_id) {
  if (!IsConcreteSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.empty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  proxy_decryptor_->CancelKeyRequest(session_id);
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

void EncryptedMediaPlayerSupportImpl::SetInitialContentDecryptionModule(
    blink::WebContentDecryptionModule* initial_cdm) {
  // Used when loading media and no pipeline/decoder attached yet.
  DCHECK(decryptor_ready_cb_.is_null());

  web_cdm_ = ToWebContentDecryptionModuleImpl(initial_cdm);
}

void EncryptedMediaPlayerSupportImpl::SetContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm) {
  // TODO(xhwang): Support setMediaKeys(0) if necessary: http://crbug.com/330324
  if (!cdm)
    return;

  web_cdm_ = ToWebContentDecryptionModuleImpl(cdm);

  if (web_cdm_ && !decryptor_ready_cb_.is_null())
    base::ResetAndReturn(&decryptor_ready_cb_)
        .Run(web_cdm_->GetDecryptor(), base::Bind(DoNothing));
}

void EncryptedMediaPlayerSupportImpl::SetContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm,
    blink::WebContentDecryptionModuleResult result) {
  // TODO(xhwang): Support setMediaKeys(0) if necessary: http://crbug.com/330324
  if (!cdm) {
    result.completeWithError(
        blink::WebContentDecryptionModuleExceptionNotSupportedError,
        0,
        "Null MediaKeys object is not supported.");
    return;
  }

  web_cdm_ = ToWebContentDecryptionModuleImpl(cdm);

  if (web_cdm_ && !decryptor_ready_cb_.is_null()) {
    base::ResetAndReturn(&decryptor_ready_cb_)
        .Run(web_cdm_->GetDecryptor(), BIND_TO_RENDER_LOOP1(
            &EncryptedMediaPlayerSupportImpl::ContentDecryptionModuleAttached,
            result));
  } else {
    // No pipeline/decoder connected, so resolve the promise. When something
    // is connected, setting the CDM will happen in SetDecryptorReadyCB().
    ContentDecryptionModuleAttached(result, true);
  }
}

void EncryptedMediaPlayerSupportImpl::ContentDecryptionModuleAttached(
    blink::WebContentDecryptionModuleResult result,
    bool success) {
  if (success) {
    result.complete();
    return;
  }

  result.completeWithError(
      blink::WebContentDecryptionModuleExceptionNotSupportedError,
      0,
      "Unable to set MediaKeys object");
}

media::SetDecryptorReadyCB
EncryptedMediaPlayerSupportImpl::CreateSetDecryptorReadyCB() {
  return BIND_TO_RENDER_LOOP(
      &EncryptedMediaPlayerSupportImpl::SetDecryptorReadyCB);
}

media::Demuxer::NeedKeyCB
EncryptedMediaPlayerSupportImpl::CreateNeedKeyCB() {
  return BIND_TO_RENDER_LOOP(&EncryptedMediaPlayerSupportImpl::OnNeedKey);
}

void EncryptedMediaPlayerSupportImpl::OnPipelineDecryptError() {
  EmeUMAHistogramCounts(current_key_system_, "DecryptError", 1);
}

void EncryptedMediaPlayerSupportImpl::OnNeedKey(const std::string& type,
                                   const std::vector<uint8>& init_data) {
  // Do not fire NeedKey event if encrypted media is not enabled.
  if (!blink::WebRuntimeFeatures::isPrefixedEncryptedMediaEnabled() &&
      !blink::WebRuntimeFeatures::isEncryptedMediaEnabled()) {
    return;
  }

  UMA_HISTOGRAM_COUNTS(kMediaEme + std::string("NeedKey"), 1);

  DCHECK(init_data_type_.empty() || type.empty() || type == init_data_type_);
  if (init_data_type_.empty())
    init_data_type_ = type;

  const uint8* init_data_ptr = init_data.empty() ? NULL : &init_data[0];
  client_->keyNeeded(
      WebString::fromUTF8(type), init_data_ptr, init_data.size());
}

void EncryptedMediaPlayerSupportImpl::OnKeyAdded(
    const std::string& session_id) {
  EmeUMAHistogramCounts(current_key_system_, "KeyAdded", 1);
  client_->keyAdded(
      WebString::fromUTF8(GetPrefixedKeySystemName(current_key_system_)),
      WebString::fromUTF8(session_id));
}

void EncryptedMediaPlayerSupportImpl::OnKeyError(const std::string& session_id,
                                    media::MediaKeys::KeyError error_code,
                                    uint32 system_code) {
  EmeUMAHistogramEnumeration(current_key_system_, "KeyError",
                             error_code, media::MediaKeys::kMaxKeyError);

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

void EncryptedMediaPlayerSupportImpl::OnKeyMessage(
    const std::string& session_id,
    const std::vector<uint8>& message,
    const GURL& destination_url) {
  DCHECK(destination_url.is_empty() || destination_url.is_valid());

  client_->keyMessage(
      WebString::fromUTF8(GetPrefixedKeySystemName(current_key_system_)),
      WebString::fromUTF8(session_id),
      message.empty() ? NULL : &message[0],
      message.size(),
      destination_url);
}

void EncryptedMediaPlayerSupportImpl::SetDecryptorReadyCB(
     const media::DecryptorReadyCB& decryptor_ready_cb) {
  // Cancels the previous decryptor request.
  if (decryptor_ready_cb.is_null()) {
    if (!decryptor_ready_cb_.is_null()) {
      base::ResetAndReturn(&decryptor_ready_cb_)
          .Run(NULL, base::Bind(DoNothing));
    }
    return;
  }

  // TODO(xhwang): Support multiple decryptor notification request (e.g. from
  // video and audio). The current implementation is okay for the current
  // media pipeline since we initialize audio and video decoders in sequence.
  // But WebMediaPlayerImpl should not depend on media pipeline's implementation
  // detail.
  DCHECK(decryptor_ready_cb_.is_null());

  // Mixed use of prefixed and unprefixed EME APIs is disallowed by Blink.
  DCHECK(!proxy_decryptor_ || !web_cdm_);

  if (proxy_decryptor_) {
    decryptor_ready_cb.Run(proxy_decryptor_->GetDecryptor(),
                           base::Bind(DoNothing));
    return;
  }

  if (web_cdm_) {
    decryptor_ready_cb.Run(web_cdm_->GetDecryptor(), base::Bind(DoNothing));
    return;
  }

  decryptor_ready_cb_ = decryptor_ready_cb;
}

}  // namespace content
