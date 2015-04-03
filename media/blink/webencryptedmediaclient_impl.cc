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
#include "media/blink/webmediaplayer_util.h"
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

// Accumulates configuration rules to determine if a feature (additional
// configuration rule) can be added to an accumulated configuration.
class ConfigState {
 public:
  ConfigState(bool was_permission_requested, bool is_permission_granted)
      : was_permission_requested_(was_permission_requested),
        is_permission_granted_(is_permission_granted) {
  }

  bool IsPermissionGranted() const {
    return is_permission_granted_;
  }

  // Permission is possible if it has not been denied.
  bool IsPermissionPossible() const {
    return is_permission_granted_ || !was_permission_requested_;
  }

  bool IsIdentifierRequired() const {
    return is_identifier_required_;
  }

  bool IsIdentifierRecommended() const {
    return is_identifier_recommended_;
  }

  // Checks whether a rule is compatible with all previously added rules.
  bool IsRuleSupported(EmeConfigRule rule) const {
    switch (rule) {
      case EmeConfigRule::NOT_SUPPORTED:
        return false;
      case EmeConfigRule::IDENTIFIER_NOT_ALLOWED:
        return !is_identifier_required_;
      case EmeConfigRule::IDENTIFIER_REQUIRED:
        // TODO(sandersd): Confirm if we should be refusing these rules when
        // permission has been denied (as the spec currently says).
        return !is_identifier_not_allowed_ && IsPermissionPossible();
      case EmeConfigRule::IDENTIFIER_RECOMMENDED:
        return true;
      case EmeConfigRule::PERSISTENCE_NOT_ALLOWED:
        return !is_persistence_required_;
      case EmeConfigRule::PERSISTENCE_REQUIRED:
        return !is_persistence_not_allowed_;
      case EmeConfigRule::IDENTIFIER_AND_PERSISTENCE_REQUIRED:
        return (!is_identifier_not_allowed_ && IsPermissionPossible() &&
                !is_persistence_not_allowed_);
      case EmeConfigRule::SUPPORTED:
        return true;
    }
    NOTREACHED();
    return false;
  }

  // Add a rule to the accumulated configuration state.
  void AddRule(EmeConfigRule rule) {
    DCHECK(IsRuleSupported(rule));
    switch (rule) {
      case EmeConfigRule::NOT_SUPPORTED:
        return;
      case EmeConfigRule::IDENTIFIER_NOT_ALLOWED:
        is_identifier_not_allowed_ = true;
        return;
      case EmeConfigRule::IDENTIFIER_REQUIRED:
        is_identifier_required_ = true;
        return;
      case EmeConfigRule::IDENTIFIER_RECOMMENDED:
        is_identifier_recommended_ = true;
        return;
      case EmeConfigRule::PERSISTENCE_NOT_ALLOWED:
        is_persistence_not_allowed_ = true;
        return;
      case EmeConfigRule::PERSISTENCE_REQUIRED:
        is_persistence_required_ = true;
        return;
      case EmeConfigRule::IDENTIFIER_AND_PERSISTENCE_REQUIRED:
        is_identifier_required_ = true;
        is_persistence_required_ = true;
        return;
      case EmeConfigRule::SUPPORTED:
        return;
    }
    NOTREACHED();
  }

 private:
  // Whether permission to use a distinctive identifier was requested. If set,
  // |is_permission_granted_| represents the final decision.
  const bool was_permission_requested_;

  // Whether permission to use a distinctive identifier has been granted.
  const bool is_permission_granted_;

  // Whether a rule has been added that requires or blocks a distinctive
  // identifier.
  bool is_identifier_required_ = false;
  bool is_identifier_not_allowed_ = false;

  // Whether a rule has been added that recommends a distinctive identifier.
  bool is_identifier_recommended_ = false;

  // Whether a rule has been added that requires or blocks persistent state.
  bool is_persistence_required_ = false;
  bool is_persistence_not_allowed_ = false;
};

static bool IsSupportedContentType(
    const KeySystems& key_systems,
    const std::string& key_system,
    EmeMediaType media_type,
    const std::string& container_mime_type,
    const std::string& codecs) {
  // TODO(sandersd): Move contentType parsing from Blink to here so that invalid
  // parameters can be rejected. http://crbug.com/417561
  std::string container_lower = base::StringToLowerASCII(container_mime_type);

  // Check that |container_mime_type| and |codecs| are supported by the CDM.
  // This check does not handle extended codecs, so extended codec information
  // is stripped.
  std::vector<std::string> codec_vector;
  net::ParseCodecString(codecs, &codec_vector, true);
  if (!key_systems.IsSupportedCodecCombination(
           key_system, media_type, container_lower, codec_vector)) {
    return false;
  }

  // Check that |container_mime_type| is supported by Chrome. This can only
  // happen if the CDM declares support for a container that Chrome does not.
  if (!net::IsSupportedMediaMimeType(container_lower)) {
    NOTREACHED();
    return false;
  }

  // Check that |codecs| are supported by Chrome. This is done primarily to
  // validate extended codecs, but it also ensures that the CDM cannot support
  // codecs that Chrome does not (which could complicate the robustness
  // algorithm).
  if (codec_vector.empty())
    return true;
  codec_vector.clear();
  net::ParseCodecString(codecs, &codec_vector, false);
  return (net::IsSupportedStrictMediaMimeType(container_lower, codec_vector) ==
          net::IsSupported);
}

static bool GetSupportedCapabilities(
    const KeySystems& key_systems,
    const std::string& key_system,
    EmeMediaType media_type,
    const blink::WebVector<blink::WebMediaKeySystemMediaCapability>&
        requested_media_capabilities,
    ConfigState* config_state,
    std::vector<blink::WebMediaKeySystemMediaCapability>*
        supported_media_capabilities) {
  // From
  // https://w3c.github.io/encrypted-media/#get-supported-capabilities-for-media-type
  // 1. Let local accumulated capabilities be a local copy of partial
  //    configuration.
  //    (Skipped as we directly update |config_state|. This is safe because we
  //    only do so when at least one requested media capability is supported.)
  // 2. Let supported media capabilities be empty.
  DCHECK_EQ(supported_media_capabilities->size(), 0ul);
  // 3. For each value in requested media capabilities:
  for (size_t i = 0; i < requested_media_capabilities.size(); i++) {
    // 3.1. Let contentType be the value's contentType member.
    // 3.2. Let robustness be the value's robustness member.
    const blink::WebMediaKeySystemMediaCapability& capability =
        requested_media_capabilities[i];
    // 3.3. If contentType is the empty string, return null.
    if (capability.mimeType.isEmpty()) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "a capability contentType was empty.";
      return false;
    }
    // 3.4-3.11. (Implemented by IsSupportedContentType().)
    if (!base::IsStringASCII(capability.mimeType) ||
        !base::IsStringASCII(capability.codecs) ||
        !IsSupportedContentType(key_systems, key_system, media_type,
                                base::UTF16ToASCII(capability.mimeType),
                                base::UTF16ToASCII(capability.codecs))) {
      continue;
    }
    // 3.12. If robustness is not the empty string, run the following steps:
    if (!capability.robustness.isEmpty()) {
      // 3.12.1. If robustness is an unrecognized value or not supported by
      //         implementation, continue to the next iteration. String
      //         comparison is case-sensitive.
      if (!base::IsStringASCII(capability.robustness))
        continue;
      EmeConfigRule robustness_rule = key_systems.GetRobustnessConfigRule(
          key_system, media_type, base::UTF16ToASCII(capability.robustness));
      if (!config_state->IsRuleSupported(robustness_rule))
        continue;
      config_state->AddRule(robustness_rule);
      // 3.12.2. Add robustness to configuration.
      //         (It's already added, we use capability as configuration.)
    }
    // 3.13. If the user agent and implementation do not support playback of
    //       encrypted media data as specified by configuration, including all
    //       media types, in combination with local accumulated capabilities,
    //       continue to the next iteration.
    //       (This is handled when adding rules to |config_state|.)
    // 3.14. Add configuration to supported media capabilities.
    supported_media_capabilities->push_back(capability);
    // 3.15. Add configuration to local accumulated capabilities.
    //       (Skipped as we directly update |config_state|.)
  }
  // 4. If supported media capabilities is empty, return null.
  if (supported_media_capabilities->empty()) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "no capabilities were supported.";
    return false;
  }
  // 5. Return media type capabilities.
  return true;
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
    const KeySystems& key_systems,
    const std::string& key_system,
    const blink::WebMediaKeySystemConfiguration& candidate,
    bool was_permission_requested,
    bool is_permission_granted,
    blink::WebMediaKeySystemConfiguration* accumulated_configuration) {
  ConfigState config_state(was_permission_requested, is_permission_granted);

  // From https://w3c.github.io/encrypted-media/#get-supported-configuration
  // 1. Let accumulated configuration be empty. (Done by caller.)
  // 2. If the initDataTypes member is present in candidate configuration, run
  //    the following steps:
  if (candidate.hasInitDataTypes) {
    // 2.1. Let supported types be empty.
    std::vector<blink::WebEncryptedMediaInitDataType> supported_types;

    // 2.2. For each value in candidate configuration's initDataTypes member:
    for (size_t i = 0; i < candidate.initDataTypes.size(); i++) {
      // 2.2.1. Let initDataType be the value.
      blink::WebEncryptedMediaInitDataType init_data_type =
          candidate.initDataTypes[i];
      // 2.2.2. If the implementation supports generating requests based on
      //        initDataType, add initDataType to supported types. String
      //        comparison is case-sensitive. The empty string is never
      //        supported.
      if (init_data_type == blink::WebEncryptedMediaInitDataType::Unknown)
        continue;
      if (key_systems.IsSupportedInitDataType(
              key_system, ConvertToEmeInitDataType(init_data_type))) {
        supported_types.push_back(init_data_type);
      }
    }

    // 2.3. If supported types is empty, return null.
    if (supported_types.empty()) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "no initDataType values were supported.";
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 2.4. Add supported types to accumulated configuration.
    accumulated_configuration->initDataTypes = supported_types;
  }

  // 3. Follow the steps for the value of candidate configuration's
  //    distinctiveIdentifier member from the following list:
  //      - "required": If the implementation does not support a persistent
  //        Distinctive Identifier in combination with accumulated
  //        configuration, return null.
  //      - "optional": Continue.
  //      - "not-allowed": If the implementation requires a Distinctive
  //        Identifier in combination with accumulated configuration, return
  //        null.
  // We also reject OPTIONAL when distinctive identifiers are ALWAYS_ENABLED and
  // permission has already been denied. This would happen anyway at step 11.
  EmeConfigRule di_rule = key_systems.GetDistinctiveIdentifierConfigRule(
      key_system, ConvertRequirement(candidate.distinctiveIdentifier));
  if (!config_state.IsRuleSupported(di_rule)) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "the distinctiveIdentifier requirement was not supported.";
    return CONFIGURATION_NOT_SUPPORTED;
  }
  config_state.AddRule(di_rule);

  // 4. Add the value of the candidate configuration's distinctiveIdentifier
  //    member to accumulated configuration.
  accumulated_configuration->distinctiveIdentifier =
      candidate.distinctiveIdentifier;

  // 5. Follow the steps for the value of candidate configuration's
  //    persistentState member from the following list:
  //      - "required": If the implementation does not support persisting state
  //        in combination with accumulated configuration, return null.
  //      - "optional": Continue.
  //      - "not-allowed": If the implementation requires persisting state in
  //        combination with accumulated configuration, return null.
  EmeConfigRule ps_rule = key_systems.GetPersistentStateConfigRule(
      key_system, ConvertRequirement(candidate.persistentState));
  if (!config_state.IsRuleSupported(ps_rule)) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "the persistentState requirement was not supported.";
    return CONFIGURATION_NOT_SUPPORTED;
  }
  config_state.AddRule(ps_rule);

  // 6. Add the value of the candidate configuration's persistentState
  //    member to accumulated configuration.
  accumulated_configuration->persistentState = candidate.persistentState;

  // 7. Follow the steps for the first matching condition from the following
  //    list:
  //      - If the sessionTypes member is present in candidate configuration,
  //        let session types be candidate configuration's sessionTypes member.
  //      - Otherwise, let session types be [ "temporary" ].
  blink::WebVector<blink::WebEncryptedMediaSessionType> session_types;
  if (candidate.hasSessionTypes) {
    session_types = candidate.sessionTypes;
  } else {
    std::vector<blink::WebEncryptedMediaSessionType> temporary(1);
    temporary[0] = blink::WebEncryptedMediaSessionType::Temporary;
    session_types = temporary;
  }

  // 8. For each value in session types:
  for (size_t i = 0; i < session_types.size(); i++) {
    // 8.1. Let session type be the value.
    blink::WebEncryptedMediaSessionType session_type = session_types[i];
    // 8.2. If the implementation does not support session type in combination
    //      with accumulated configuration, return null.
    // 8.3. If session type is "persistent-license" or
    //      "persistent-release-message", follow the steps for accumulated
    //      configuration's persistentState value from the following list:
    //        - "required": Continue.
    //        - "optional": Change accumulated configuration's persistentState
    //          value to "required".
    //        - "not-allowed": Return null.
    EmeConfigRule session_type_rule = EmeConfigRule::NOT_SUPPORTED;
    switch (session_type) {
      case blink::WebEncryptedMediaSessionType::Unknown:
        DVLOG(2) << "Rejecting requested configuration because "
                 << "a required session type was not recognized.";
        return CONFIGURATION_NOT_SUPPORTED;
      case blink::WebEncryptedMediaSessionType::Temporary:
        session_type_rule = EmeConfigRule::SUPPORTED;
        break;
      case blink::WebEncryptedMediaSessionType::PersistentLicense:
        session_type_rule =
            key_systems.GetPersistentLicenseSessionConfigRule(key_system);
        break;
      case blink::WebEncryptedMediaSessionType::PersistentReleaseMessage:
        session_type_rule =
            key_systems.GetPersistentReleaseMessageSessionConfigRule(
                key_system);
        break;
    }
    if (!config_state.IsRuleSupported(session_type_rule)) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "a required session type was not supported.";
      return CONFIGURATION_NOT_SUPPORTED;
    }
    config_state.AddRule(session_type_rule);
  }

  // 9. Add session types to accumulated configuration.
  accumulated_configuration->sessionTypes = session_types;

  // 10. If the videoCapabilities member is present in candidate configuration:
  if (candidate.hasVideoCapabilities) {
    // 10.1. Let video capabilities be the result of executing the Get Supported
    //       Capabilities for Media Type algorithm on Video, candidate
    //       configuration's videoCapabilities member, and accumulated
    //       configuration.
    // 10.2. If video capabilities is null, return null.
    std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities;
    if (!GetSupportedCapabilities(key_systems, key_system, EmeMediaType::VIDEO,
                                  candidate.videoCapabilities,
                                  &config_state, &video_capabilities)) {
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 10.3. Add video capabilities to accumulated configuration.
    accumulated_configuration->videoCapabilities = video_capabilities;
  }

  // 11. If the audioCapabilities member is present in candidate configuration:
  if (candidate.hasAudioCapabilities) {
    // 11.1. Let audio capabilities be the result of executing the Get Supported
    //       Capabilities for Media Type algorithm on Audio, candidate
    //       configuration's audioCapabilities member, and accumulated
    //       configuration.
    // 11.2. If audio capabilities is null, return null.
    std::vector<blink::WebMediaKeySystemMediaCapability> audio_capabilities;
    if (!GetSupportedCapabilities(key_systems, key_system, EmeMediaType::AUDIO,
                                  candidate.audioCapabilities,
                                  &config_state, &audio_capabilities)) {
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 11.3. Add audio capabilities to accumulated configuration.
    accumulated_configuration->audioCapabilities = audio_capabilities;
  }

  // 12. If accumulated configuration's distinctiveIdentifier value is
  //     "optional", follow the steps for the first matching condition from the
  //     following list:
  //       - If the implementation requires a Distinctive Identifier for any of
  //         the combinations in accumulated configuration, change accumulated
  //         configuration's distinctiveIdentifier value to "required".
  //       - Otherwise, change accumulated configuration's distinctiveIdentifier
  //         value to "not-allowed".
  if (accumulated_configuration->distinctiveIdentifier ==
      blink::WebMediaKeySystemConfiguration::Requirement::Optional) {
    EmeConfigRule not_allowed_rule =
        key_systems.GetDistinctiveIdentifierConfigRule(
            key_system, EME_FEATURE_NOT_ALLOWED);
    EmeConfigRule required_rule =
        key_systems.GetDistinctiveIdentifierConfigRule(
            key_system, EME_FEATURE_REQUIRED);
    bool not_allowed_supported = config_state.IsRuleSupported(not_allowed_rule);
    bool required_supported = config_state.IsRuleSupported(required_rule);
    // If a distinctive identifier is recommend and that is a possible outcome,
    // prefer that.
    if (required_supported &&
        config_state.IsIdentifierRecommended() &&
        config_state.IsPermissionPossible()) {
      not_allowed_supported = false;
    }
    if (not_allowed_supported) {
      accumulated_configuration->distinctiveIdentifier =
          blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed;
      config_state.AddRule(not_allowed_rule);
    } else if (required_supported) {
      accumulated_configuration->distinctiveIdentifier =
          blink::WebMediaKeySystemConfiguration::Requirement::Required;
      config_state.AddRule(required_rule);
    } else {
      // We should not have passed step 3.
      NOTREACHED();
      return CONFIGURATION_NOT_SUPPORTED;
    }
  }

  // 13. If accumulated configuration's persistentState value is "optional",
  //     follow the steps for the first matching condition from the following
  //     list:
  //       - If the implementation requires persisting state for any of the
  //         combinations in accumulated configuration, change accumulated
  //         configuration's persistentState value to "required".
  //       - Otherwise, change accumulated configuration's persistentState value
  //         to "not-allowed".
  if (accumulated_configuration->persistentState ==
      blink::WebMediaKeySystemConfiguration::Requirement::Optional) {
    EmeConfigRule not_allowed_rule =
        key_systems.GetPersistentStateConfigRule(
            key_system, EME_FEATURE_NOT_ALLOWED);
    EmeConfigRule required_rule =
        key_systems.GetPersistentStateConfigRule(
            key_system, EME_FEATURE_REQUIRED);
    // |distinctiveIdentifier| should not be affected after it is decided.
    DCHECK(not_allowed_rule == EmeConfigRule::NOT_SUPPORTED ||
           not_allowed_rule == EmeConfigRule::PERSISTENCE_NOT_ALLOWED);
    DCHECK(required_rule == EmeConfigRule::NOT_SUPPORTED ||
           required_rule == EmeConfigRule::PERSISTENCE_REQUIRED);
    bool not_allowed_supported =
        config_state.IsRuleSupported(not_allowed_rule);
    bool required_supported =
        config_state.IsRuleSupported(required_rule);
    if (not_allowed_supported) {
      accumulated_configuration->persistentState =
          blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed;
      config_state.AddRule(not_allowed_rule);
    } else if (required_supported) {
      accumulated_configuration->persistentState =
          blink::WebMediaKeySystemConfiguration::Requirement::Required;
      config_state.AddRule(required_rule);
    } else {
      // We should not have passed step 5.
      NOTREACHED();
      return CONFIGURATION_NOT_SUPPORTED;
    }
  }

  // 14. If implementation in the configuration specified by the combination of
  //     the values in accumulated configuration is not supported or not allowed
  //     in the origin, return null.
  // 15. If accumulated configuration's distinctiveIdentifier value is
  //     "required", [prompt the user for consent].
  if (accumulated_configuration->distinctiveIdentifier ==
      blink::WebMediaKeySystemConfiguration::Requirement::Required) {
    // The caller is responsible for resolving what to do if permission is
    // required but has been denied (it should treat it as NOT_SUPPORTED).
    if (!config_state.IsPermissionGranted())
      return CONFIGURATION_REQUIRES_PERMISSION;
  }

  // 16. If the label member is present in candidate configuration, add the
  //     value of the candidate configuration's label member to accumulated
  //     configuration.
  accumulated_configuration->label = candidate.label;

  // 17. Return accumulated configuration.
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
    : key_systems_(KeySystems::GetInstance()),
      cdm_factory_(cdm_factory.Pass()),
      media_permission_(media_permission),
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

  if (!key_systems_.IsSupportedKeySystem(key_system)) {
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
        key_systems_, key_system, candidate_configuration,
        was_permission_requested, is_permission_granted,
        &accumulated_configuration);
    switch (supported) {
      case CONFIGURATION_NOT_SUPPORTED:
        continue;
      case CONFIGURATION_REQUIRES_PERMISSION:
        if (was_permission_requested) {
          DVLOG(2) << "Rejecting requested configuration because "
                   << "permission was denied.";
          continue;
        }
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
    bool allow_distinctive_identifier,
    bool allow_persistent_state,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebContentDecryptionModuleResult result) {
  WebContentDecryptionModuleImpl::Create(cdm_factory_.get(), key_system,
                                         allow_distinctive_identifier,
                                         allow_persistent_state,
                                         security_origin, result);
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
