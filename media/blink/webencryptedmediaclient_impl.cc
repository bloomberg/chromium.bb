// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webencryptedmediaclient_impl.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/key_systems.h"
#include "media/base/media_permission.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaRequest.h"
#include "third_party/WebKit/public/platform/WebMediaKeySystemConfiguration.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "webcontentdecryptionmodule_impl.h"
#include "webcontentdecryptionmoduleaccess_impl.h"

namespace media {

// These names are used by UMA.
const char kKeySystemSupportUMAPrefix[] =
    "Media.EME.RequestMediaKeySystemAccess.";

static bool IsSupportedContentType(
    const std::string& key_system,
    const std::string& mime_type,
    const std::string& codecs) {
  // Per RFC 6838, "Both top-level type and subtype names are case-insensitive."
  // TODO(sandersd): Check that |container| matches the capability:
  //   - audioCapabilitys: audio/mp4 or audio/webm.
  //   - videoCapabilitys: video/mp4 or video/webm.
  // http://crbug.com/457384.
  std::string container = base::StringToLowerASCII(mime_type);

  // Check that |codecs| are supported as specified (e.g. "mp4a.40.2").
  std::vector<std::string> codec_vector;
  net::ParseCodecString(codecs, &codec_vector, false);
  if (!net::AreSupportedMediaCodecs(codec_vector))
    return false;

  // IsSupportedKeySystemWithMediaMimeType() only works with base codecs
  // (e.g. "mp4a"), so reparse |codecs| to get the base only.
  codec_vector.clear();
  net::ParseCodecString(codecs, &codec_vector, true);
  return IsSupportedKeySystemWithMediaMimeType(container, codec_vector,
                                               key_system);
}

static bool GetSupportedConfiguration(
    const std::string& key_system,
    const blink::WebMediaKeySystemConfiguration& candidate,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebMediaKeySystemConfiguration* accumulated_configuration) {
  if (!candidate.initDataTypes.isEmpty()) {
    std::vector<blink::WebString> init_data_types;

    for (size_t i = 0; i < candidate.initDataTypes.size(); i++) {
      const blink::WebString& init_data_type = candidate.initDataTypes[i];
      if (init_data_type.isEmpty())
        return false;
      if (base::IsStringASCII(init_data_type) &&
          IsSupportedKeySystemWithInitDataType(
              key_system, base::UTF16ToASCII(init_data_type))) {
        init_data_types.push_back(init_data_type);
      }
    }

    if (init_data_types.empty())
      return false;

    accumulated_configuration->initDataTypes = init_data_types;
  }

  // TODO(sandersd): Implement distinctiveIdentifier and persistentState checks.
  if (candidate.distinctiveIdentifier !=
      blink::WebMediaKeySystemConfiguration::Requirement::Optional ||
      candidate.persistentState !=
      blink::WebMediaKeySystemConfiguration::Requirement::Optional) {
    return false;
  }

  if (!candidate.audioCapabilities.isEmpty()) {
    std::vector<blink::WebMediaKeySystemMediaCapability> audio_capabilities;

    for (size_t i = 0; i < candidate.audioCapabilities.size(); i++) {
      const blink::WebMediaKeySystemMediaCapability& capabilities =
          candidate.audioCapabilities[i];
      if (capabilities.mimeType.isEmpty())
        return false;
      if (!base::IsStringASCII(capabilities.mimeType) ||
          !base::IsStringASCII(capabilities.codecs) ||
          !IsSupportedContentType(
              key_system, base::UTF16ToASCII(capabilities.mimeType),
              base::UTF16ToASCII(capabilities.codecs))) {
        continue;
      }
      // TODO(sandersd): Support robustness.
      if (!capabilities.robustness.isEmpty())
        continue;
      audio_capabilities.push_back(capabilities);
    }

    if (audio_capabilities.empty())
      return false;

    accumulated_configuration->audioCapabilities = audio_capabilities;
  }

  if (!candidate.videoCapabilities.isEmpty()) {
    std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities;

    for (size_t i = 0; i < candidate.videoCapabilities.size(); i++) {
      const blink::WebMediaKeySystemMediaCapability& capabilities =
          candidate.videoCapabilities[i];
      if (capabilities.mimeType.isEmpty())
        return false;
      if (!base::IsStringASCII(capabilities.mimeType) ||
          !base::IsStringASCII(capabilities.codecs) ||
          !IsSupportedContentType(
              key_system, base::UTF16ToASCII(capabilities.mimeType),
              base::UTF16ToASCII(capabilities.codecs))) {
        continue;
      }
      // TODO(sandersd): Support robustness.
      if (!capabilities.robustness.isEmpty())
        continue;
      video_capabilities.push_back(capabilities);
    }

    if (video_capabilities.empty())
      return false;

    accumulated_configuration->videoCapabilities = video_capabilities;
  }

  // TODO(sandersd): Prompt for distinctive identifiers and/or persistent state
  // if required. Make sure that future checks are silent.
  // http://crbug.com/446263.

  return true;
}

// Report usage of key system to UMA. There are 2 different counts logged:
// 1. The key system is requested.
// 2. The requested key system and options are supported.
// Each stat is only reported once per renderer frame per key system.
// Note that WebEncryptedMediaClientImpl is only created once by each
// renderer frame.
class WebEncryptedMediaClientImpl::Reporter {
 public:
  enum KeySystemSupportStatus {
    KEY_SYSTEM_REQUESTED = 0,
    KEY_SYSTEM_SUPPORTED = 1,
    KEY_SYSTEM_SUPPORT_STATUS_COUNT
  };

  explicit Reporter(const std::string& key_system_for_uma)
      : uma_name_(kKeySystemSupportUMAPrefix + key_system_for_uma),
        is_request_reported_(false),
        is_support_reported_(false) {}
  ~Reporter() {}

  void ReportRequested() {
    if (is_request_reported_)
      return;
    Report(KEY_SYSTEM_REQUESTED);
    is_request_reported_ = true;
  }

  void ReportSupported() {
    DCHECK(is_request_reported_);
    if (is_support_reported_)
      return;
    Report(KEY_SYSTEM_SUPPORTED);
    is_support_reported_ = true;
  }

 private:
  void Report(KeySystemSupportStatus status) {
    // Not using UMA_HISTOGRAM_ENUMERATION directly because UMA_* macros
    // require the names to be constant throughout the process' lifetime.
    base::LinearHistogram::FactoryGet(
        uma_name_, 1, KEY_SYSTEM_SUPPORT_STATUS_COUNT,
        KEY_SYSTEM_SUPPORT_STATUS_COUNT + 1,
        base::Histogram::kUmaTargetedHistogramFlag)->Add(status);
  }

  const std::string uma_name_;
  bool is_request_reported_;
  bool is_support_reported_;
};

WebEncryptedMediaClientImpl::WebEncryptedMediaClientImpl(
    scoped_ptr<CdmFactory> cdm_factory,
    MediaPermission* media_permission)
    : cdm_factory_(cdm_factory.Pass()), weak_factory_(this) {
  // TODO(sandersd): Use |media_permission| to check for media permissions in
  // this class.
  DCHECK(media_permission);
}

WebEncryptedMediaClientImpl::~WebEncryptedMediaClientImpl() {
}

void WebEncryptedMediaClientImpl::requestMediaKeySystemAccess(
    blink::WebEncryptedMediaRequest request) {
  // TODO(jrummell): This should be asynchronous.

  // Continued from requestMediaKeySystemAccess(), step 7, from
  // https://w3c.github.io/encrypted-media/#requestmediakeysystemaccess
  //
  // 7.1 If keySystem is not one of the Key Systems supported by the user
  //     agent, reject promise with with a new DOMException whose name is
  //     NotSupportedError. String comparison is case-sensitive.
  if (!base::IsStringASCII(request.keySystem())) {
    request.requestNotSupported("Only ASCII keySystems are supported");
    return;
  }

  std::string key_system = base::UTF16ToASCII(request.keySystem());

  // Report this request to the appropriate Reporter.
  Reporter* reporter = GetReporter(key_system);
  reporter->ReportRequested();

  if (!IsConcreteSupportedKeySystem(key_system)) {
    request.requestNotSupported("Unsupported keySystem");
    return;
  }

  // 7.2 Let implementation be the implementation of keySystem.
  // 7.3 For each value in supportedConfigurations, run the GetSupported
  //     Configuration algorithm and if successful, resolve promise with access
  //     and abort these steps.
  const blink::WebVector<blink::WebMediaKeySystemConfiguration>&
      configurations = request.supportedConfigurations();

  // TODO(sandersd): Remove once Blink requires the configurations parameter for
  // requestMediaKeySystemAccess().
  if (configurations.isEmpty()) {
    reporter->ReportSupported();
    request.requestSucceeded(WebContentDecryptionModuleAccessImpl::Create(
        request.keySystem(), blink::WebMediaKeySystemConfiguration(),
        request.securityOrigin(), weak_factory_.GetWeakPtr()));
    return;
  }

  for (size_t i = 0; i < configurations.size(); i++) {
    const blink::WebMediaKeySystemConfiguration& candidate = configurations[i];
    blink::WebMediaKeySystemConfiguration accumulated_configuration;
    if (GetSupportedConfiguration(key_system, candidate,
                                  request.securityOrigin(),
                                  &accumulated_configuration)) {
      reporter->ReportSupported();
      request.requestSucceeded(WebContentDecryptionModuleAccessImpl::Create(
          request.keySystem(), accumulated_configuration,
          request.securityOrigin(), weak_factory_.GetWeakPtr()));
      return;
    }
  }

  // 7.4 Reject promise with a new DOMException whose name is NotSupportedError.
  request.requestNotSupported(
      "None of the requested configurations were supported.");
}

void WebEncryptedMediaClientImpl::CreateCdm(
    const blink::WebString& key_system,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebContentDecryptionModuleResult result) {
  WebContentDecryptionModuleImpl::Create(cdm_factory_.get(), security_origin,
                                         key_system, result);
}

// Lazily create Reporters.
WebEncryptedMediaClientImpl::Reporter* WebEncryptedMediaClientImpl::GetReporter(
    const std::string& key_system) {
  std::string uma_name = GetKeySystemNameForUMA(key_system);
  Reporter* reporter = reporters_.get(uma_name);
  if (reporter != nullptr)
    return reporter;

  // Reporter not found, so create one.
  auto result =
      reporters_.add(uma_name, make_scoped_ptr(new Reporter(uma_name)));
  DCHECK(result.second);
  return result.first->second;
}

}  // namespace media
