// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "key_system_config_selector.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/key_systems.h"
#include "media/base/media_permission.h"
#include "media/blink/webmediaplayer_util.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebMediaKeySystemConfiguration.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "url/gurl.h"

namespace media {

namespace {

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

}  // namespace

struct KeySystemConfigSelector::SelectionRequest {
  std::string key_system;
  blink::WebVector<blink::WebMediaKeySystemConfiguration>
      candidate_configurations;
  blink::WebSecurityOrigin security_origin;
  base::Callback<void(const blink::WebMediaKeySystemConfiguration&)>
      succeeded_cb;
  base::Callback<void(const blink::WebString&)> not_supported_cb;
  bool was_permission_requested = false;
  bool is_permission_granted = false;
};

// Accumulates configuration rules to determine if a feature (additional
// configuration rule) can be added to an accumulated configuration.
class KeySystemConfigSelector::ConfigState {
 public:
  ConfigState(bool was_permission_requested, bool is_permission_granted)
      : was_permission_requested_(was_permission_requested),
        is_permission_granted_(is_permission_granted) {}

  bool IsPermissionGranted() const { return is_permission_granted_; }

  // Permission is possible if it has not been denied.
  bool IsPermissionPossible() const {
    return is_permission_granted_ || !was_permission_requested_;
  }

  bool IsIdentifierRequired() const { return is_identifier_required_; }

  bool IsIdentifierRecommended() const { return is_identifier_recommended_; }

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

  DISALLOW_COPY_AND_ASSIGN(ConfigState);
};

KeySystemConfigSelector::KeySystemConfigSelector(
    const KeySystems* key_systems,
    MediaPermission* media_permission)
    : key_systems_(key_systems),
      media_permission_(media_permission),
      weak_factory_(this) {
  DCHECK(key_systems_);
  DCHECK(media_permission_);
}

KeySystemConfigSelector::~KeySystemConfigSelector() {
}

bool KeySystemConfigSelector::IsSupportedContentType(
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
  if (!key_systems_->IsSupportedCodecCombination(
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

bool KeySystemConfigSelector::GetSupportedCapabilities(
    const std::string& key_system,
    EmeMediaType media_type,
    const blink::WebVector<blink::WebMediaKeySystemMediaCapability>&
        requested_media_capabilities,
    KeySystemConfigSelector::ConfigState* config_state,
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
        !IsSupportedContentType(key_system, media_type,
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
      EmeConfigRule robustness_rule = key_systems_->GetRobustnessConfigRule(
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

KeySystemConfigSelector::ConfigurationSupport
KeySystemConfigSelector::GetSupportedConfiguration(
    const std::string& key_system,
    const blink::WebMediaKeySystemConfiguration& candidate,
    ConfigState* config_state,
    blink::WebMediaKeySystemConfiguration* accumulated_configuration) {
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
      if (key_systems_->IsSupportedInitDataType(
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
  EmeConfigRule di_rule = key_systems_->GetDistinctiveIdentifierConfigRule(
      key_system, ConvertRequirement(candidate.distinctiveIdentifier));
  if (!config_state->IsRuleSupported(di_rule)) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "the distinctiveIdentifier requirement was not supported.";
    return CONFIGURATION_NOT_SUPPORTED;
  }
  config_state->AddRule(di_rule);

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
  EmeConfigRule ps_rule = key_systems_->GetPersistentStateConfigRule(
      key_system, ConvertRequirement(candidate.persistentState));
  if (!config_state->IsRuleSupported(ps_rule)) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "the persistentState requirement was not supported.";
    return CONFIGURATION_NOT_SUPPORTED;
  }
  config_state->AddRule(ps_rule);

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
            key_systems_->GetPersistentLicenseSessionConfigRule(key_system);
        break;
      case blink::WebEncryptedMediaSessionType::PersistentReleaseMessage:
        session_type_rule =
            key_systems_->GetPersistentReleaseMessageSessionConfigRule(
                key_system);
        break;
    }
    if (!config_state->IsRuleSupported(session_type_rule)) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "a required session type was not supported.";
      return CONFIGURATION_NOT_SUPPORTED;
    }
    config_state->AddRule(session_type_rule);
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
    if (!GetSupportedCapabilities(key_system, EmeMediaType::VIDEO,
                                  candidate.videoCapabilities, config_state,
                                  &video_capabilities)) {
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
    if (!GetSupportedCapabilities(key_system, EmeMediaType::AUDIO,
                                  candidate.audioCapabilities, config_state,
                                  &audio_capabilities)) {
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
        key_systems_->GetDistinctiveIdentifierConfigRule(
            key_system, EME_FEATURE_NOT_ALLOWED);
    EmeConfigRule required_rule =
        key_systems_->GetDistinctiveIdentifierConfigRule(key_system,
                                                         EME_FEATURE_REQUIRED);
    bool not_allowed_supported =
        config_state->IsRuleSupported(not_allowed_rule);
    bool required_supported = config_state->IsRuleSupported(required_rule);
    // If a distinctive identifier is recommend and that is a possible outcome,
    // prefer that.
    if (required_supported && config_state->IsIdentifierRecommended() &&
        config_state->IsPermissionPossible()) {
      not_allowed_supported = false;
    }
    if (not_allowed_supported) {
      accumulated_configuration->distinctiveIdentifier =
          blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed;
      config_state->AddRule(not_allowed_rule);
    } else if (required_supported) {
      accumulated_configuration->distinctiveIdentifier =
          blink::WebMediaKeySystemConfiguration::Requirement::Required;
      config_state->AddRule(required_rule);
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
    EmeConfigRule not_allowed_rule = key_systems_->GetPersistentStateConfigRule(
        key_system, EME_FEATURE_NOT_ALLOWED);
    EmeConfigRule required_rule = key_systems_->GetPersistentStateConfigRule(
        key_system, EME_FEATURE_REQUIRED);
    // |distinctiveIdentifier| should not be affected after it is decided.
    DCHECK(not_allowed_rule == EmeConfigRule::NOT_SUPPORTED ||
           not_allowed_rule == EmeConfigRule::PERSISTENCE_NOT_ALLOWED);
    DCHECK(required_rule == EmeConfigRule::NOT_SUPPORTED ||
           required_rule == EmeConfigRule::PERSISTENCE_REQUIRED);
    bool not_allowed_supported =
        config_state->IsRuleSupported(not_allowed_rule);
    bool required_supported = config_state->IsRuleSupported(required_rule);
    if (not_allowed_supported) {
      accumulated_configuration->persistentState =
          blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed;
      config_state->AddRule(not_allowed_rule);
    } else if (required_supported) {
      accumulated_configuration->persistentState =
          blink::WebMediaKeySystemConfiguration::Requirement::Required;
      config_state->AddRule(required_rule);
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
    if (!config_state->IsPermissionGranted())
      return CONFIGURATION_REQUIRES_PERMISSION;
  }

  // 16. If the label member is present in candidate configuration, add the
  //     value of the candidate configuration's label member to accumulated
  //     configuration.
  accumulated_configuration->label = candidate.label;

  // 17. Return accumulated configuration.
  return CONFIGURATION_SUPPORTED;
}

void KeySystemConfigSelector::SelectConfig(
    const blink::WebString& key_system,
    const blink::WebVector<blink::WebMediaKeySystemConfiguration>&
        candidate_configurations,
    const blink::WebSecurityOrigin& security_origin,
    base::Callback<void(const blink::WebMediaKeySystemConfiguration&)>
        succeeded_cb,
    base::Callback<void(const blink::WebString&)> not_supported_cb) {
  // Continued from requestMediaKeySystemAccess(), step 7, from
  // https://w3c.github.io/encrypted-media/#requestmediakeysystemaccess
  //
  // 7.1. If keySystem is not one of the Key Systems supported by the user
  //      agent, reject promise with with a new DOMException whose name is
  //      NotSupportedError. String comparison is case-sensitive.
  if (!base::IsStringASCII(key_system)) {
    not_supported_cb.Run("Only ASCII keySystems are supported");
    return;
  }

  std::string key_system_ascii = base::UTF16ToASCII(key_system);
  if (!key_systems_->IsSupportedKeySystem(key_system_ascii)) {
    not_supported_cb.Run("Unsupported keySystem");
    return;
  }

  // 7.2-7.4. Implemented by OnSelectConfig().
  // TODO(sandersd): This should be async, ideally not on the main thread.
  scoped_ptr<SelectionRequest> request(new SelectionRequest());
  request->key_system = key_system_ascii;
  request->candidate_configurations = candidate_configurations;
  request->security_origin = security_origin;
  request->succeeded_cb = succeeded_cb;
  request->not_supported_cb = not_supported_cb;
  SelectConfigInternal(request.Pass());
}

void KeySystemConfigSelector::SelectConfigInternal(
    scoped_ptr<SelectionRequest> request) {
  // Continued from requestMediaKeySystemAccess(), step 7.1, from
  // https://w3c.github.io/encrypted-media/#requestmediakeysystemaccess
  //
  // 7.2. Let implementation be the implementation of keySystem.
  //      (|key_systems_| fills this role.)
  // 7.3. For each value in supportedConfigurations:
  for (size_t i = 0; i < request->candidate_configurations.size(); i++) {
    // 7.3.1. Let candidate configuration be the value.
    // 7.3.2. Let supported configuration be the result of executing the Get
    //        Supported Configuration algorithm on implementation, candidate
    //        configuration, and origin.
    // 7.3.3. If supported configuration is not null, [initialize and return a
    //        new MediaKeySystemAccess object.]
    ConfigState config_state(request->was_permission_requested,
                             request->is_permission_granted);
    blink::WebMediaKeySystemConfiguration accumulated_configuration;
    ConfigurationSupport support = GetSupportedConfiguration(
        request->key_system, request->candidate_configurations[i],
        &config_state, &accumulated_configuration);
    switch (support) {
      case CONFIGURATION_NOT_SUPPORTED:
        continue;
      case CONFIGURATION_REQUIRES_PERMISSION:
        if (request->was_permission_requested) {
          DVLOG(2) << "Rejecting requested configuration because "
                   << "permission was denied.";
          continue;
        }
        media_permission_->RequestPermission(
            MediaPermission::PROTECTED_MEDIA_IDENTIFIER,
            GURL(request->security_origin.toString()),
            base::Bind(&KeySystemConfigSelector::OnPermissionResult,
                       weak_factory_.GetWeakPtr(), base::Passed(&request)));
        return;
      case CONFIGURATION_SUPPORTED:
        request->succeeded_cb.Run(accumulated_configuration);
        return;
    }
  }

  // 7.4. Reject promise with a new DOMException whose name is
  //      NotSupportedError.
  request->not_supported_cb.Run(
      "None of the requested configurations were supported.");
}

void KeySystemConfigSelector::OnPermissionResult(
    scoped_ptr<SelectionRequest> request,
    bool is_permission_granted) {
  request->was_permission_requested = true;
  request->is_permission_granted = is_permission_granted;
  SelectConfigInternal(request.Pass());
}

}  // namespace media
