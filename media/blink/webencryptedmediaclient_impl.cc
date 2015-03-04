// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webencryptedmediaclient_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/key_systems.h"
#include "media/base/media_permission.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "media/blink/webcontentdecryptionmoduleaccess_impl.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaRequest.h"
#include "third_party/WebKit/public/platform/WebMediaKeySystemConfiguration.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace media {

// These names are used by UMA.
const char kKeySystemSupportUMAPrefix[] =
    "Media.EME.RequestMediaKeySystemAccess.";

enum ConfigurationSupport {
  CONFIGURATION_NOT_SUPPORTED,
  CONFIGURATION_REQUIRES_PERMISSION,
  CONFIGURATION_SUPPORTED,
};

static bool IsSupportedContentType(const std::string& key_system,
                                   const std::string& mime_type,
                                   const std::string& codecs) {
  // TODO(sandersd): Move contentType parsing from Blink to here so that invalid
  // parameters can be rejected. http://crbug.com/417561
  // TODO(sandersd): Pass in the media type (audio or video) and check that the
  // container type matches. http://crbug.com/457384
  std::string container = base::StringToLowerASCII(mime_type);

  // Check that |codecs| are supported by the CDM. This check does not handle
  // extended codecs, so extended codec information is stripped.
  // TODO(sandersd): Reject codecs that do not match the media type.
  // http://crbug.com/457386
  std::vector<std::string> codec_vector;
  net::ParseCodecString(codecs, &codec_vector, true);
  if (!IsSupportedKeySystemWithMediaMimeType(container, codec_vector,
                                             key_system)) {
    return false;
  }

  // Check that |codecs| are supported by Chrome. This is done primarily to
  // validate extended codecs, but it also ensures that the CDM cannot support
  // codecs that Chrome does not (which would be bad because it would require
  // considering the accumulated configuration, and could affect whether secure
  // decode is required).
  // TODO(sandersd): Reject ambiguous codecs. http://crbug.com/374751
  codec_vector.clear();
  net::ParseCodecString(codecs, &codec_vector, false);
  return net::AreSupportedMediaCodecs(codec_vector);
}

static bool GetSupportedCapabilities(
    const std::string& key_system,
    const blink::WebVector<blink::WebMediaKeySystemMediaCapability>&
        capabilities,
    std::vector<blink::WebMediaKeySystemMediaCapability>*
        media_type_capabilities) {
  // From
  // https://w3c.github.io/encrypted-media/#get-supported-capabilities-for-media-type
  // 1. Let accumulated capabilities be partial configuration.
  //    (Skipped as there are no configuration-based codec restrictions.)
  // 2. Let media type capabilities be empty.
  DCHECK_EQ(media_type_capabilities->size(), 0ul);
  // 3. For each value in capabilities:
  for (size_t i = 0; i < capabilities.size(); i++) {
    // 3.1. Let contentType be the value's contentType member.
    // 3.2. Let robustness be the value's robustness member.
    const blink::WebMediaKeySystemMediaCapability& capability = capabilities[i];
    // 3.3. If contentType is the empty string, return null.
    if (capability.mimeType.isEmpty())
      return false;
    // 3.4-3.11. (Implemented by IsSupportedContentType().)
    if (!base::IsStringASCII(capability.mimeType) ||
        !base::IsStringASCII(capability.codecs) ||
        !IsSupportedContentType(key_system,
                                base::UTF16ToASCII(capability.mimeType),
                                base::UTF16ToASCII(capability.codecs))) {
      continue;
    }
    // 3.12. If robustness is not the empty string, run the following steps:
    //       (Robustness is not supported.)
    // TODO(sandersd): Implement robustness. http://crbug.com/442586
    if (!capability.robustness.isEmpty()) {
      LOG(WARNING) << "Configuration rejected because rubustness strings are "
                   << "not yet supported.";
      continue;
    }
    // 3.13. If the user agent and implementation do not support playback of
    //       encrypted media data as specified by configuration, including all
    //       media types, in combination with accumulated capabilities,
    //       continue to the next iteration.
    //       (Skipped as there are no configuration-based codec restrictions.)
    // 3.14. Add configuration to media type capabilities.
    media_type_capabilities->push_back(capability);
    // 3.15. Add configuration to accumulated capabilities.
    //       (Skipped as there are no configuration-based codec restrictions.)
  }
  // 4. If media type capabilities is empty, return null.
  // 5. Return media type capabilities.
  return !media_type_capabilities->empty();
}

static EmeFeatureRequirement ConvertRequirement(
    blink::WebMediaKeySystemConfiguration::Requirement requirement) {
  switch (requirement) {
    case blink::WebMediaKeySystemConfiguration::Requirement::Required:
      return EME_FEATURE_REQUIRED;
    case blink::WebMediaKeySystemConfiguration::Requirement::Optional:
      return EME_FEATURE_OPTIONAL;
    case blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed:
      return EME_FEATURE_NOT_ALLOWED;
  }

  NOTREACHED();
  return EME_FEATURE_NOT_ALLOWED;
}

static ConfigurationSupport GetSupportedConfiguration(
    const std::string& key_system,
    const blink::WebMediaKeySystemConfiguration& candidate,
    blink::WebMediaKeySystemConfiguration* accumulated_configuration,
    bool was_permission_requested,
    bool is_permission_granted) {
  DCHECK(was_permission_requested || !is_permission_granted);

  // It is possible to obtain user permission unless permission was already
  // requested and denied.
  bool is_permission_possible =
      !was_permission_requested || is_permission_granted;

  // From https://w3c.github.io/encrypted-media/#get-supported-configuration
  // 1. Let accumulated configuration be empty. (Done by caller.)
  // 2. If candidate configuration's initDataTypes attribute is not empty, run
  //    the following steps:
  blink::WebVector<blink::WebEncryptedMediaInitDataType> init_data_types =
      candidate.getInitDataTypes();
  if (!init_data_types.isEmpty()) {
    // 2.1. Let supported types be empty.
    std::vector<blink::WebEncryptedMediaInitDataType> supported_types;

    // 2.2. For each value in candidate configuration's initDataTypes attribute:
    for (size_t i = 0; i < init_data_types.size(); i++) {
      // 2.2.1. Let initDataType be the value.
      blink::WebEncryptedMediaInitDataType init_data_type = init_data_types[i];
      // 2.2.2. If initDataType is the empty string, return null.
      if (init_data_type == blink::WebEncryptedMediaInitDataType::Unknown)
        continue;
      // 2.2.3. If the implementation supports generating requests based on
      //        initDataType, add initDataType to supported types. String
      //        comparison is case-sensitive.
      // TODO(jrummell): |init_data_type| should be an enum all the way through
      // Chromium. http://crbug.com/417440
      std::string init_data_type_as_ascii = "unknown";
      switch (init_data_type) {
        case blink::WebEncryptedMediaInitDataType::Cenc:
          init_data_type_as_ascii = "cenc";
          break;
        case blink::WebEncryptedMediaInitDataType::Keyids:
          init_data_type_as_ascii = "keyids";
          break;
        case blink::WebEncryptedMediaInitDataType::Webm:
          init_data_type_as_ascii = "webm";
          break;
        case blink::WebEncryptedMediaInitDataType::Unknown:
          NOTREACHED();
          break;
      }
      if (IsSupportedKeySystemWithInitDataType(key_system,
                                               init_data_type_as_ascii)) {
        supported_types.push_back(init_data_type);
      }
    }

    // 2.3. If supported types is empty, return null.
    if (supported_types.empty())
      return CONFIGURATION_NOT_SUPPORTED;

    // 2.4. Add supported types to accumulated configuration.
    accumulated_configuration->setInitDataTypes(supported_types);
  }

  // 3. Follow the steps for the value of candidate configuration's
  //    distinctiveIdentifier attribute from the following list:
  //     - "required": If the implementation does not support a persistent
  //       Distinctive Identifier in combination with accumulated configuration,
  //       return null.
  //     - "optional": Continue.
  //     - "not-allowed": If the implementation requires a Distinctive
  //       Identifier in combination with accumulated configuration, return
  //       null.
  EmeFeatureRequirement di_requirement =
      ConvertRequirement(candidate.distinctiveIdentifier);
  if (!IsDistinctiveIdentifierRequirementSupported(key_system, di_requirement,
                                                   is_permission_possible)) {
    return CONFIGURATION_NOT_SUPPORTED;
  }

  // 4. Add the value of the candidate configuration's distinctiveIdentifier
  //    attribute to accumulated configuration.
  accumulated_configuration->distinctiveIdentifier =
      candidate.distinctiveIdentifier;

  // 5. Follow the steps for the value of candidate configuration's
  //    persistentState attribute from the following list:
  //     - "required": If the implementation does not support persisting state
  //       in combination with accumulated configuration, return null.
  //     - "optional": Continue.
  //     - "not-allowed": If the implementation requires persisting state in
  //       combination with accumulated configuration, return null.
  EmeFeatureRequirement ps_requirement =
      ConvertRequirement(candidate.persistentState);
  if (!IsPersistentStateRequirementSupported(key_system, ps_requirement,
                                             is_permission_possible)) {
    return CONFIGURATION_NOT_SUPPORTED;
  }

  // 6. Add the value of the candidate configuration's persistentState
  //    attribute to accumulated configuration.
  accumulated_configuration->persistentState = candidate.persistentState;

  // 7. If candidate configuration's videoCapabilities attribute is not empty,
  //    run the following steps:
  if (!candidate.videoCapabilities.isEmpty()) {
    // 7.1. Let video capabilities be the result of executing the Get Supported
    //      Capabilities for Media Type algorithm on Video, candidate
    //      configuration's videoCapabilities attribute, and accumulated
    //      configuration.
    // 7.2. If video capabilities is null, return null.
    std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities;
    if (!GetSupportedCapabilities(key_system, candidate.videoCapabilities,
                                  &video_capabilities)) {
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 7.3. Add video capabilities to accumulated configuration.
    accumulated_configuration->videoCapabilities = video_capabilities;
  }

  // 8. If candidate configuration's audioCapabilities attribute is not empty,
  //    run the following steps:
  if (!candidate.audioCapabilities.isEmpty()) {
    // 8.1. Let audio capabilities be the result of executing the Get Supported
    //      Capabilities for Media Type algorithm on Audio, candidate
    //      configuration's audioCapabilities attribute, and accumulated
    //      configuration.
    // 8.2. If audio capabilities is null, return null.
    std::vector<blink::WebMediaKeySystemMediaCapability> audio_capabilities;
    if (!GetSupportedCapabilities(key_system, candidate.audioCapabilities,
                                  &audio_capabilities)) {
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 8.3. Add audio capabilities to accumulated configuration.
    accumulated_configuration->audioCapabilities = audio_capabilities;
  }

  // 9. If accumulated configuration's distinctiveIdentifier value is
  //    "optional", follow the steps for the first matching condition from the
  //    following list:
  //      - If the implementation requires a Distinctive Identifier for any of
  //        the combinations in accumulated configuration, change accumulated
  //        configuration's distinctiveIdentifier value to "required".
  //      - Otherwise, change accumulated configuration's distinctiveIdentifier
  //        value to "not-allowed".
  //    (Without robustness support, capabilities do not affect this.)
  // TODO(sandersd): Implement robustness. http://crbug.com/442586
  if (accumulated_configuration->distinctiveIdentifier ==
      blink::WebMediaKeySystemConfiguration::Requirement::Optional) {
    if (IsDistinctiveIdentifierRequirementSupported(
            key_system, EME_FEATURE_NOT_ALLOWED, is_permission_possible)) {
      accumulated_configuration->distinctiveIdentifier =
          blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed;
    } else {
      accumulated_configuration->distinctiveIdentifier =
          blink::WebMediaKeySystemConfiguration::Requirement::Required;
    }
  }

  // 10. If accumulated configuration's persistentState value is "optional",
  //     follow the steps for the first matching condition from the following
  //     list:
  //       - If the implementation requires persisting state for any of the
  //         combinations in accumulated configuration, change accumulated
  //         configuration's persistentState value to "required".
  //       - Otherwise, change accumulated configuration's persistentState value
  //         to "not-allowed".
  if (accumulated_configuration->persistentState ==
      blink::WebMediaKeySystemConfiguration::Requirement::Optional) {
    if (IsPersistentStateRequirementSupported(
            key_system, EME_FEATURE_NOT_ALLOWED, is_permission_possible)) {
      accumulated_configuration->persistentState =
          blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed;
    } else {
      accumulated_configuration->persistentState =
          blink::WebMediaKeySystemConfiguration::Requirement::Required;
    }
  }

  // 11. If implementation in the configuration specified by the combination of
  //     the values in accumulated configuration is not supported or not allowed
  //     in the origin, return null.
  di_requirement =
      ConvertRequirement(accumulated_configuration->distinctiveIdentifier);
  if (!IsDistinctiveIdentifierRequirementSupported(key_system, di_requirement,
                                                   is_permission_granted)) {
    if (was_permission_requested) {
      // The optional permission was requested and denied.
      // TODO(sandersd): Avoid the need for this logic - crbug.com/460616.
      DCHECK(candidate.distinctiveIdentifier ==
             blink::WebMediaKeySystemConfiguration::Requirement::Optional);
      DCHECK(di_requirement == EME_FEATURE_REQUIRED);
      DCHECK(!is_permission_granted);
      accumulated_configuration->distinctiveIdentifier =
          blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed;
    } else {
      return CONFIGURATION_REQUIRES_PERMISSION;
    }
  }

  ps_requirement =
      ConvertRequirement(accumulated_configuration->persistentState);
  if (!IsPersistentStateRequirementSupported(key_system, ps_requirement,
                                             is_permission_granted)) {
    DCHECK(!was_permission_requested);  // Should have failed at step 5.
    return CONFIGURATION_REQUIRES_PERMISSION;
  }

  // 12. Return accumulated configuration.
  //     (As an extra step, we record the available session types so that
  //     createSession() can be synchronous.)
  std::vector<blink::WebEncryptedMediaSessionType> session_types;
  session_types.push_back(blink::WebEncryptedMediaSessionType::Temporary);
  if (accumulated_configuration->persistentState ==
      blink::WebMediaKeySystemConfiguration::Requirement::Required) {
    if (IsPersistentLicenseSessionSupported(key_system,
                                            is_permission_granted)) {
      session_types.push_back(
          blink::WebEncryptedMediaSessionType::PersistentLicense);
    }
    if (IsPersistentReleaseMessageSessionSupported(key_system,
                                                   is_permission_granted)) {
      session_types.push_back(
          blink::WebEncryptedMediaSessionType::PersistentReleaseMessage);
    }
  }
  accumulated_configuration->setSessionTypes(session_types);

  return CONFIGURATION_SUPPORTED;
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
    : cdm_factory_(cdm_factory.Pass()), media_permission_(media_permission),
      weak_factory_(this) {
  DCHECK(media_permission);
}

WebEncryptedMediaClientImpl::~WebEncryptedMediaClientImpl() {
}

void WebEncryptedMediaClientImpl::requestMediaKeySystemAccess(
    blink::WebEncryptedMediaRequest request) {
  // TODO(jrummell): This should be asynchronous, ideally not on the main
  // thread.

  // Continued from requestMediaKeySystemAccess(), step 7, from
  // https://w3c.github.io/encrypted-media/#requestmediakeysystemaccess
  //
  // 7.1. If keySystem is not one of the Key Systems supported by the user
  //      agent, reject promise with with a new DOMException whose name is
  //      NotSupportedError. String comparison is case-sensitive.
  if (!base::IsStringASCII(request.keySystem())) {
    request.requestNotSupported("Only ASCII keySystems are supported");
    return;
  }

  // Report this request to the UMA.
  std::string key_system = base::UTF16ToASCII(request.keySystem());
  GetReporter(key_system)->ReportRequested();

  if (!IsSupportedKeySystem(key_system)) {
    request.requestNotSupported("Unsupported keySystem");
    return;
  }

  // 7.2-7.4. Implemented by SelectSupportedConfiguration().
  SelectSupportedConfiguration(request, false, false);
}

void WebEncryptedMediaClientImpl::SelectSupportedConfiguration(
    blink::WebEncryptedMediaRequest request,
    bool was_permission_requested,
    bool is_permission_granted) {
  // Continued from requestMediaKeySystemAccess(), step 7.1, from
  // https://w3c.github.io/encrypted-media/#requestmediakeysystemaccess
  //
  // 7.2. Let implementation be the implementation of keySystem.
  std::string key_system = base::UTF16ToASCII(request.keySystem());

  // 7.3. For each value in supportedConfigurations:
  const blink::WebVector<blink::WebMediaKeySystemConfiguration>&
      configurations = request.supportedConfigurations();
  for (size_t i = 0; i < configurations.size(); i++) {
    // 7.3.1. Let candidate configuration be the value.
    const blink::WebMediaKeySystemConfiguration& candidate_configuration =
        configurations[i];
    // 7.3.2. Let supported configuration be the result of executing the Get
    //        Supported Configuration algorithm on implementation, candidate
    //        configuration, and origin.
    // 7.3.3. If supported configuration is not null, [initialize and return a
    //        new MediaKeySystemAccess object.]
    blink::WebMediaKeySystemConfiguration accumulated_configuration;
    ConfigurationSupport supported = GetSupportedConfiguration(
        key_system, candidate_configuration, &accumulated_configuration,
        was_permission_requested, is_permission_granted);
    switch (supported) {
      case CONFIGURATION_NOT_SUPPORTED:
        continue;
      case CONFIGURATION_REQUIRES_PERMISSION:
        DCHECK(!was_permission_requested);
        media_permission_->RequestPermission(
            MediaPermission::PROTECTED_MEDIA_IDENTIFIER,
            GURL(request.securityOrigin().toString()),
            // Try again with |was_permission_requested| true and
            // |is_permission_granted| the value of the permission.
            base::Bind(
                &WebEncryptedMediaClientImpl::SelectSupportedConfiguration,
                weak_factory_.GetWeakPtr(), request, true));
        return;
      case CONFIGURATION_SUPPORTED:
        // Report that this request succeeded to the UMA.
        GetReporter(key_system)->ReportSupported();
        request.requestSucceeded(WebContentDecryptionModuleAccessImpl::Create(
            request.keySystem(), accumulated_configuration,
            request.securityOrigin(), weak_factory_.GetWeakPtr()));
        return;
    }
  }

  // 7.4. Reject promise with a new DOMException whose name is
  //      NotSupportedError.
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
