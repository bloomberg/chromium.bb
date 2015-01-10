// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webencryptedmediaclient_impl.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/key_systems.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaRequest.h"
#include "third_party/WebKit/public/platform/WebMediaKeySystemConfiguration.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "webcontentdecryptionmoduleaccess_impl.h"

namespace media {

static bool IsSupportedContentType(
    const std::string& key_system,
    const std::string& mime_type,
    const std::string& codecs) {
  // Per RFC 6838, "Both top-level type and subtype names are case-insensitive."
  // TODO(sandersd): Check that |container| matches the capability:
  //   - audioCapabilitys: audio/mp4 or audio/webm.
  //   - videoCapabilitys: video/mp4 or video/webm.
  // http://crbug.com/429781.
  std::string container = base::StringToLowerASCII(mime_type);

  // TODO(sandersd): Strict checking for codecs. http://crbug.com/374751.
  bool strip_codec_suffixes = !net::IsStrictMediaMimeType(container);
  std::vector<std::string> codec_vector;
  net::ParseCodecString(codecs, &codec_vector, strip_codec_suffixes);
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

WebEncryptedMediaClientImpl::WebEncryptedMediaClientImpl(
    scoped_ptr<CdmFactory> cdm_factory)
    : cdm_factory_(cdm_factory.Pass()) {
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
  if (!IsConcreteSupportedKeySystem(key_system)) {
    request.requestNotSupported("Unsupported keySystem");
    return;
  }

  // 7.2 Let implementation be the implementation of keySystem.
  // 7.3 Follow the steps for the first matching condition from the following
  //     list:
  //       - If supportedConfigurations was not provided, run the Is Key System
  //         Supported? algorithm and if successful, resolve promise with access
  //         and abort these steps.
  // TODO(sandersd): Remove pending the resolution of
  // https://github.com/w3c/encrypted-media/issues/1.
  const blink::WebVector<blink::WebMediaKeySystemConfiguration>&
      configurations = request.supportedConfigurations();
  if (configurations.isEmpty()) {
    request.requestSucceeded(WebContentDecryptionModuleAccessImpl::Create(
        request.keySystem(), request.securityOrigin(), cdm_factory_.get()));
    return;
  }

  //       - Otherwise, for each value in supportedConfigurations, run the
  //         GetSuppored Configuration algorithm and if successful, resolve
  //         promise with access and abort these steps.
  for (size_t i = 0; i < configurations.size(); i++) {
    const blink::WebMediaKeySystemConfiguration& candidate = configurations[i];
    blink::WebMediaKeySystemConfiguration accumulated_configuration;
    if (GetSupportedConfiguration(key_system, candidate,
                                  request.securityOrigin(),
                                  &accumulated_configuration)) {
      // TODO(sandersd): Pass the accumulated configuration along.
      // http://crbug.com/447059.
      request.requestSucceeded(WebContentDecryptionModuleAccessImpl::Create(
          request.keySystem(), request.securityOrigin(), cdm_factory_.get()));
      return;
    }
  }

  // 7.4 Reject promise with a new DOMException whose name is NotSupportedError.
  request.requestNotSupported(
      "None of the requested configurations were supported.");
}

}  // namespace media
