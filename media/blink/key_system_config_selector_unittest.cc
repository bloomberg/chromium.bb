// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "media/base/eme_constants.h"
#include "media/base/key_systems.h"
#include "media/base/media_permission.h"
#include "media/blink/key_system_config_selector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_encrypted_media_types.h"
#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/public/platform/web_string.h"

namespace media {

namespace {

using MediaKeysRequirement = blink::WebMediaKeySystemConfiguration::Requirement;

// Key system strings. Clear Key support is hardcoded in KeySystemConfigSelector
// so kClearKeyKeySystem is the real key system string. The rest key system
// strings are for testing purpose only.
const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kSupportedKeySystem[] = "keysystem.test.supported";
const char kUnsupportedKeySystem[] = "keysystem.test.unsupported";

// Robustness strings for kSupportedKeySystem.
const char kSupportedRobustness[] = "supported";
const char kRecommendIdentifierRobustness[] = "recommend_identifier";
const char kRequireIdentifierRobustness[] = "require_identifier";
const char kUnsupportedRobustness[] = "unsupported";

// Test container mime types. The supported ones are real container mime types
// since we check with MimeUtil.
const char kSupportedVideoContainer[] = "video/webm";
const char kSupportedAudioContainer[] = "audio/webm";
const char kUnsupportedContainer[] = "video/foo";

// The supported ones must be real codec(s) since we check with MimeUtil.
// TODO(sandersd): Extended codec variants (requires proprietary codec support).
// TODO(xhwang): Platform Opus is not available on all Android versions, where
// some encrypted Opus related tests may fail. See PlatformHasOpusSupport()
// for more details.
const char kSupportedAudioCodec[] = "opus";
const char kSupportedVideoCodec[] = "vp8";
const char kUnsupportedCodec[] = "foo";
const char kUnsupportedCodecs[] = "vp8,foo";
const char kSupportedVideoCodecs[] = "vp8,vp8";

// The IDL for MediaKeySystemConfiguration specifies some defaults, so
// create a config object that mimics what would be created if an empty
// dictionary was passed in.
blink::WebMediaKeySystemConfiguration EmptyConfiguration() {
  // http://w3c.github.io/encrypted-media/#mediakeysystemconfiguration-dictionary
  // If this member (sessionTypes) is not present when the dictionary
  // is passed to requestMediaKeySystemAccess(), the dictionary will
  // be treated as if this member is set to [ "temporary" ].
  std::vector<blink::WebEncryptedMediaSessionType> session_types;
  session_types.push_back(blink::WebEncryptedMediaSessionType::kTemporary);

  blink::WebMediaKeySystemConfiguration config;
  config.label = "";
  config.session_types = session_types;
  return config;
}

// EME spec requires that at least one of |video_capabilities| and
// |audio_capabilities| be specified. Add a single valid audio capability
// to the EmptyConfiguration().
blink::WebMediaKeySystemConfiguration UsableConfiguration() {
  // Blink code parses the contentType into mimeType and codecs, so mimic
  // that here.
  std::vector<blink::WebMediaKeySystemMediaCapability> audio_capabilities(1);
  audio_capabilities[0].mime_type = kSupportedAudioContainer;
  audio_capabilities[0].codecs = kSupportedAudioCodec;

  auto config = EmptyConfiguration();
  config.audio_capabilities = audio_capabilities;
  return config;
}

class FakeKeySystems : public KeySystems {
 public:
  ~FakeKeySystems() override = default;

  bool IsSupportedKeySystem(const std::string& key_system) const override {
    // Based on EME spec, Clear Key key system is always supported.
    if (key_system == kSupportedKeySystem || key_system == kClearKeyKeySystem)
      return true;
    return false;
  }

  // TODO(sandersd): Move implementation into KeySystemConfigSelector?
  bool IsSupportedInitDataType(const std::string& key_system,
                               EmeInitDataType init_data_type) const override {
    switch (init_data_type) {
      case EmeInitDataType::UNKNOWN:
        return false;
      case EmeInitDataType::WEBM:
        return init_data_type_webm_supported_;
      case EmeInitDataType::CENC:
        return init_data_type_cenc_supported_;
      case EmeInitDataType::KEYIDS:
        return init_data_type_keyids_supported_;
    }
    NOTREACHED();
    return false;
  }

  bool IsEncryptionSchemeSupported(
      const std::string& key_system,
      EncryptionMode encryption_scheme) const override {
    // TODO(crbug.com/658026): Implement this once value passed from blink.
    NOTREACHED();
    return false;
  }

  // TODO(xhwang): Simulate secure codec and add test cases.
  EmeConfigRule GetContentTypeConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& container_mime_type,
      const std::vector<std::string>& codecs) const override {
    if (container_mime_type == kUnsupportedContainer)
      return EmeConfigRule::NOT_SUPPORTED;
    switch (media_type) {
      case EmeMediaType::AUDIO:
        DCHECK_EQ(kSupportedAudioContainer, container_mime_type);
        break;
      case EmeMediaType::VIDEO:
        DCHECK_EQ(kSupportedVideoContainer, container_mime_type);
        break;
    }
    for (const std::string& codec : codecs) {
      if (codec == kUnsupportedCodec)
        return EmeConfigRule::NOT_SUPPORTED;
      switch (media_type) {
        case EmeMediaType::AUDIO:
          DCHECK_EQ(kSupportedAudioCodec, codec);
          break;
        case EmeMediaType::VIDEO:
          DCHECK_EQ(kSupportedVideoCodec, codec);
          break;
      }
    }
    return EmeConfigRule::SUPPORTED;
  }

  EmeConfigRule GetRobustnessConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& requested_robustness) const override {
    if (requested_robustness.empty())
      return EmeConfigRule::SUPPORTED;
    if (requested_robustness == kUnsupportedRobustness)
      return EmeConfigRule::NOT_SUPPORTED;
    if (requested_robustness == kRequireIdentifierRobustness)
      return EmeConfigRule::IDENTIFIER_REQUIRED;
    if (requested_robustness == kRecommendIdentifierRobustness)
      return EmeConfigRule::IDENTIFIER_RECOMMENDED;
    if (requested_robustness == kSupportedRobustness)
      return EmeConfigRule::SUPPORTED;

    NOTREACHED();
    return EmeConfigRule::NOT_SUPPORTED;
  }

  EmeSessionTypeSupport GetPersistentLicenseSessionSupport(
      const std::string& key_system) const override {
    return persistent_license;
  }

  EmeSessionTypeSupport GetPersistentReleaseMessageSessionSupport(
      const std::string& key_system) const override {
    return persistent_release_message;
  }

  EmeFeatureSupport GetPersistentStateSupport(
      const std::string& key_system) const override {
    return persistent_state;
  }

  EmeFeatureSupport GetDistinctiveIdentifierSupport(
      const std::string& key_system) const override {
    return distinctive_identifier;
  }

  bool init_data_type_webm_supported_ = false;
  bool init_data_type_cenc_supported_ = false;
  bool init_data_type_keyids_supported_ = false;

  // INVALID so that they must be set in any test that needs them.
  EmeSessionTypeSupport persistent_license = EmeSessionTypeSupport::INVALID;
  EmeSessionTypeSupport persistent_release_message =
      EmeSessionTypeSupport::INVALID;

  // Every test implicitly requires these, so they must be set. They are set to
  // values that are likely to cause tests to fail if they are accidentally
  // depended on. Test cases explicitly depending on them should set them, as
  // the default values may be changed.
  EmeFeatureSupport persistent_state = EmeFeatureSupport::NOT_SUPPORTED;
  EmeFeatureSupport distinctive_identifier = EmeFeatureSupport::REQUESTABLE;
};

class FakeMediaPermission : public MediaPermission {
 public:
  // MediaPermission implementation.
  void HasPermission(Type type,
                     const PermissionStatusCB& permission_status_cb) override {
    permission_status_cb.Run(is_granted);
  }

  void RequestPermission(
      Type type,
      const PermissionStatusCB& permission_status_cb) override {
    requests++;
    permission_status_cb.Run(is_granted);
  }

  bool IsEncryptedMediaEnabled() override { return is_encrypted_media_enabled; }

  int requests = 0;
  bool is_granted = false;
  bool is_encrypted_media_enabled = true;
};

}  // namespace

class KeySystemConfigSelectorTest : public testing::Test {
 public:
  KeySystemConfigSelectorTest()
      : key_systems_(new FakeKeySystems()),
        media_permission_(new FakeMediaPermission()) {}

  void SelectConfig() {
    media_permission_->requests = 0;
    succeeded_count_ = 0;
    not_supported_count_ = 0;
    KeySystemConfigSelector(key_systems_.get(), media_permission_.get())
        .SelectConfig(key_system_, configs_,
                      base::Bind(&KeySystemConfigSelectorTest::OnSucceeded,
                                 base::Unretained(this)),
                      base::Bind(&KeySystemConfigSelectorTest::OnNotSupported,
                                 base::Unretained(this)));
  }

  void SelectConfigReturnsConfig() {
    SelectConfig();
    EXPECT_EQ(0, media_permission_->requests);
    EXPECT_EQ(1, succeeded_count_);
    EXPECT_EQ(0, not_supported_count_);
    ASSERT_TRUE(succeeded_count_ != 0);
  }

  void SelectConfigReturnsError() {
    SelectConfig();
    EXPECT_EQ(0, media_permission_->requests);
    EXPECT_EQ(0, succeeded_count_);
    EXPECT_EQ(1, not_supported_count_);
    ASSERT_TRUE(not_supported_count_ != 0);
  }

  void SelectConfigRequestsPermissionAndReturnsConfig() {
    SelectConfig();
    EXPECT_EQ(1, media_permission_->requests);
    EXPECT_EQ(1, succeeded_count_);
    EXPECT_EQ(0, not_supported_count_);
    ASSERT_TRUE(media_permission_->requests != 0 && succeeded_count_ != 0);
  }

  void SelectConfigRequestsPermissionAndReturnsError() {
    SelectConfig();
    EXPECT_EQ(1, media_permission_->requests);
    EXPECT_EQ(0, succeeded_count_);
    EXPECT_EQ(1, not_supported_count_);
    ASSERT_TRUE(media_permission_->requests != 0 && not_supported_count_ != 0);
  }

  void OnSucceeded(const blink::WebMediaKeySystemConfiguration& result,
                   const CdmConfig& cdm_config) {
    succeeded_count_++;
    config_ = result;
  }

  void OnNotSupported() { not_supported_count_++; }

  std::unique_ptr<FakeKeySystems> key_systems_;
  std::unique_ptr<FakeMediaPermission> media_permission_;

  // Held values for the call to SelectConfig().
  blink::WebString key_system_ =
      blink::WebString::FromUTF8(kSupportedKeySystem);
  std::vector<blink::WebMediaKeySystemConfiguration> configs_;

  // Holds the last successful accumulated configuration.
  blink::WebMediaKeySystemConfiguration config_;

  int succeeded_count_;
  int not_supported_count_;

  DISALLOW_COPY_AND_ASSIGN(KeySystemConfigSelectorTest);
};

// --- Basics ---

TEST_F(KeySystemConfigSelectorTest, NoConfigs) {
  SelectConfigReturnsError();
}

TEST_F(KeySystemConfigSelectorTest, DefaultConfig) {
  auto config = EmptyConfiguration();

  // label = "";
  ASSERT_EQ("", config.label);

  // initDataTypes = [];
  ASSERT_EQ(0u, config.init_data_types.size());

  // audioCapabilities = [];
  ASSERT_EQ(0u, config.audio_capabilities.size());

  // videoCapabilities = [];
  ASSERT_EQ(0u, config.video_capabilities.size());

  // distinctiveIdentifier = "optional";
  ASSERT_EQ(MediaKeysRequirement::kOptional, config.distinctive_identifier);

  // persistentState = "optional";
  ASSERT_EQ(MediaKeysRequirement::kOptional, config.persistent_state);

  // If this member is not present when the dictionary is passed to
  // requestMediaKeySystemAccess(), the dictionary will be treated as
  // if this member is set to [ "temporary" ].
  ASSERT_EQ(1u, config.session_types.size());
  ASSERT_EQ(blink::WebEncryptedMediaSessionType::kTemporary,
            config.session_types[0]);
}

TEST_F(KeySystemConfigSelectorTest, EmptyConfig) {
  // EME spec requires that at least one of |video_capabilities| and
  // |audio_capabilities| be specified.
  configs_.push_back(EmptyConfiguration());
  SelectConfigReturnsError();
}

// Most of the tests below assume that the the usable config is valid.
// Tests that touch |video_capabilities| and/or |audio_capabilities| can
// modify the empty config.

TEST_F(KeySystemConfigSelectorTest, UsableConfig) {
  configs_.push_back(UsableConfiguration());

  SelectConfigReturnsConfig();
  EXPECT_EQ("", config_.label);
  EXPECT_TRUE(config_.init_data_types.IsEmpty());
  EXPECT_EQ(1u, config_.audio_capabilities.size());
  EXPECT_TRUE(config_.video_capabilities.IsEmpty());
  EXPECT_EQ(MediaKeysRequirement::kNotAllowed, config_.distinctive_identifier);
  EXPECT_EQ(MediaKeysRequirement::kNotAllowed, config_.persistent_state);
  ASSERT_EQ(1u, config_.session_types.size());
  EXPECT_EQ(blink::WebEncryptedMediaSessionType::kTemporary,
            config_.session_types[0]);
}

TEST_F(KeySystemConfigSelectorTest, Label) {
  auto config = UsableConfiguration();
  config.label = "foo";
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  EXPECT_EQ("foo", config_.label);
}

// --- keySystem ---
// Empty is not tested because the empty check is in Blink.

TEST_F(KeySystemConfigSelectorTest, KeySystem_NonAscii) {
  key_system_ = "\xde\xad\xbe\xef";
  configs_.push_back(UsableConfiguration());
  SelectConfigReturnsError();
}

TEST_F(KeySystemConfigSelectorTest, KeySystem_Unsupported) {
  key_system_ = kUnsupportedKeySystem;
  configs_.push_back(UsableConfiguration());
  SelectConfigReturnsError();
}

TEST_F(KeySystemConfigSelectorTest, KeySystem_ClearKey) {
  key_system_ = kClearKeyKeySystem;
  configs_.push_back(UsableConfiguration());
  SelectConfigReturnsConfig();
}

// --- Disable EncryptedMedia ---

TEST_F(KeySystemConfigSelectorTest, EncryptedMediaDisabled_ClearKey) {
  media_permission_->is_encrypted_media_enabled = false;

  // Clear Key key system is always supported.
  key_system_ = kClearKeyKeySystem;
  configs_.push_back(UsableConfiguration());
  SelectConfigReturnsConfig();
}

TEST_F(KeySystemConfigSelectorTest, EncryptedMediaDisabled_Supported) {
  media_permission_->is_encrypted_media_enabled = false;

  // Other key systems are not supported.
  key_system_ = kSupportedKeySystem;
  configs_.push_back(UsableConfiguration());
  SelectConfigReturnsError();
}

// --- initDataTypes ---

TEST_F(KeySystemConfigSelectorTest, InitDataTypes_Empty) {
  auto config = UsableConfiguration();
  configs_.push_back(config);

  SelectConfigReturnsConfig();
}

TEST_F(KeySystemConfigSelectorTest, InitDataTypes_NoneSupported) {
  key_systems_->init_data_type_webm_supported_ = true;

  std::vector<blink::WebEncryptedMediaInitDataType> init_data_types;
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::kUnknown);
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::kCenc);

  auto config = UsableConfiguration();
  config.init_data_types = init_data_types;
  configs_.push_back(config);

  SelectConfigReturnsError();
}

TEST_F(KeySystemConfigSelectorTest, InitDataTypes_SubsetSupported) {
  key_systems_->init_data_type_webm_supported_ = true;

  std::vector<blink::WebEncryptedMediaInitDataType> init_data_types;
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::kUnknown);
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::kCenc);
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::kWebm);

  auto config = UsableConfiguration();
  config.init_data_types = init_data_types;
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  ASSERT_EQ(1u, config_.init_data_types.size());
  EXPECT_EQ(blink::WebEncryptedMediaInitDataType::kWebm,
            config_.init_data_types[0]);
}

// --- distinctiveIdentifier ---

TEST_F(KeySystemConfigSelectorTest, DistinctiveIdentifier_Default) {
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  auto config = UsableConfiguration();
  config.distinctive_identifier = MediaKeysRequirement::kOptional;
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  EXPECT_EQ(MediaKeysRequirement::kNotAllowed, config_.distinctive_identifier);
}

TEST_F(KeySystemConfigSelectorTest, DistinctiveIdentifier_Forced) {
  media_permission_->is_granted = true;
  key_systems_->distinctive_identifier = EmeFeatureSupport::ALWAYS_ENABLED;

  auto config = UsableConfiguration();
  config.distinctive_identifier = MediaKeysRequirement::kOptional;
  configs_.push_back(config);

  SelectConfigRequestsPermissionAndReturnsConfig();
  EXPECT_EQ(MediaKeysRequirement::kRequired, config_.distinctive_identifier);
}

TEST_F(KeySystemConfigSelectorTest, DistinctiveIdentifier_Blocked) {
  key_systems_->distinctive_identifier = EmeFeatureSupport::NOT_SUPPORTED;

  auto config = UsableConfiguration();
  config.distinctive_identifier = MediaKeysRequirement::kRequired;
  configs_.push_back(config);

  SelectConfigReturnsError();
}

TEST_F(KeySystemConfigSelectorTest, DistinctiveIdentifier_RequestsPermission) {
  media_permission_->is_granted = true;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  auto config = UsableConfiguration();
  config.distinctive_identifier = MediaKeysRequirement::kRequired;
  configs_.push_back(config);

  SelectConfigRequestsPermissionAndReturnsConfig();
  EXPECT_EQ(MediaKeysRequirement::kRequired, config_.distinctive_identifier);
}

TEST_F(KeySystemConfigSelectorTest, DistinctiveIdentifier_RespectsPermission) {
  media_permission_->is_granted = false;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  auto config = UsableConfiguration();
  config.distinctive_identifier = MediaKeysRequirement::kRequired;
  configs_.push_back(config);

  SelectConfigRequestsPermissionAndReturnsError();
}

// --- persistentState ---

TEST_F(KeySystemConfigSelectorTest, PersistentState_Default) {
  key_systems_->persistent_state = EmeFeatureSupport::REQUESTABLE;

  auto config = UsableConfiguration();
  config.persistent_state = MediaKeysRequirement::kOptional;
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  EXPECT_EQ(MediaKeysRequirement::kNotAllowed, config_.persistent_state);
}

TEST_F(KeySystemConfigSelectorTest, PersistentState_Forced) {
  key_systems_->persistent_state = EmeFeatureSupport::ALWAYS_ENABLED;

  auto config = UsableConfiguration();
  config.persistent_state = MediaKeysRequirement::kOptional;
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  EXPECT_EQ(MediaKeysRequirement::kRequired, config_.persistent_state);
}

TEST_F(KeySystemConfigSelectorTest, PersistentState_Blocked) {
  key_systems_->persistent_state = EmeFeatureSupport::ALWAYS_ENABLED;

  auto config = UsableConfiguration();
  config.persistent_state = MediaKeysRequirement::kNotAllowed;
  configs_.push_back(config);

  SelectConfigReturnsError();
}

// --- sessionTypes ---

TEST_F(KeySystemConfigSelectorTest, SessionTypes_Empty) {
  auto config = UsableConfiguration();

  // Usable configuration has [ "temporary" ].
  std::vector<blink::WebEncryptedMediaSessionType> session_types;
  config.session_types = session_types;

  configs_.push_back(config);

  SelectConfigReturnsConfig();
  EXPECT_TRUE(config_.session_types.IsEmpty());
}

TEST_F(KeySystemConfigSelectorTest, SessionTypes_SubsetSupported) {
  // Allow persistent state, as it would be required to be successful.
  key_systems_->persistent_state = EmeFeatureSupport::REQUESTABLE;
  key_systems_->persistent_license = EmeSessionTypeSupport::NOT_SUPPORTED;

  std::vector<blink::WebEncryptedMediaSessionType> session_types;
  session_types.push_back(blink::WebEncryptedMediaSessionType::kTemporary);
  session_types.push_back(
      blink::WebEncryptedMediaSessionType::kPersistentLicense);

  auto config = UsableConfiguration();
  config.session_types = session_types;
  configs_.push_back(config);

  SelectConfigReturnsError();
}

TEST_F(KeySystemConfigSelectorTest, SessionTypes_AllSupported) {
  // Allow persistent state, and expect it to be required.
  key_systems_->persistent_state = EmeFeatureSupport::REQUESTABLE;
  key_systems_->persistent_license = EmeSessionTypeSupport::SUPPORTED;

  std::vector<blink::WebEncryptedMediaSessionType> session_types;
  session_types.push_back(blink::WebEncryptedMediaSessionType::kTemporary);
  session_types.push_back(
      blink::WebEncryptedMediaSessionType::kPersistentLicense);

  auto config = UsableConfiguration();
  config.persistent_state = MediaKeysRequirement::kOptional;
  config.session_types = session_types;
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  EXPECT_EQ(MediaKeysRequirement::kRequired, config_.persistent_state);
  ASSERT_EQ(2u, config_.session_types.size());
  EXPECT_EQ(blink::WebEncryptedMediaSessionType::kTemporary,
            config_.session_types[0]);
  EXPECT_EQ(blink::WebEncryptedMediaSessionType::kPersistentLicense,
            config_.session_types[1]);
}

TEST_F(KeySystemConfigSelectorTest, SessionTypes_PermissionCanBeRequired) {
  media_permission_->is_granted = true;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;
  key_systems_->persistent_state = EmeFeatureSupport::REQUESTABLE;
  key_systems_->persistent_license =
      EmeSessionTypeSupport::SUPPORTED_WITH_IDENTIFIER;

  std::vector<blink::WebEncryptedMediaSessionType> session_types;
  session_types.push_back(
      blink::WebEncryptedMediaSessionType::kPersistentLicense);

  auto config = UsableConfiguration();
  config.distinctive_identifier = MediaKeysRequirement::kOptional;
  config.persistent_state = MediaKeysRequirement::kOptional;
  config.session_types = session_types;
  configs_.push_back(config);

  SelectConfigRequestsPermissionAndReturnsConfig();
  EXPECT_EQ(MediaKeysRequirement::kRequired, config_.distinctive_identifier);
}

// --- videoCapabilities ---

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Empty) {
  auto config = UsableConfiguration();
  configs_.push_back(config);

  SelectConfigReturnsConfig();
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_NoneSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(2);
  video_capabilities[0].content_type = "a";
  video_capabilities[0].mime_type = kUnsupportedContainer;
  video_capabilities[1].content_type = "b";
  video_capabilities[1].mime_type = kSupportedVideoContainer;
  video_capabilities[1].codecs = kUnsupportedCodec;

  auto config = EmptyConfiguration();
  config.video_capabilities = video_capabilities;
  configs_.push_back(config);

  SelectConfigReturnsError();
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_SubsetSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(2);
  video_capabilities[0].content_type = "a";
  video_capabilities[0].mime_type = kUnsupportedContainer;
  video_capabilities[1].content_type = "b";
  video_capabilities[1].mime_type = kSupportedVideoContainer;
  video_capabilities[1].codecs = kSupportedVideoCodec;

  auto config = EmptyConfiguration();
  config.video_capabilities = video_capabilities;
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  ASSERT_EQ(1u, config_.video_capabilities.size());
  EXPECT_EQ("b", config_.video_capabilities[0].content_type);
  EXPECT_EQ(kSupportedVideoContainer, config_.video_capabilities[0].mime_type);
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_AllSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(2);
  video_capabilities[0].content_type = "a";
  video_capabilities[0].mime_type = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodecs;
  video_capabilities[1].content_type = "b";
  video_capabilities[1].mime_type = kSupportedVideoContainer;
  video_capabilities[1].codecs = kSupportedVideoCodecs;

  auto config = EmptyConfiguration();
  config.video_capabilities = video_capabilities;
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  ASSERT_EQ(2u, config_.video_capabilities.size());
  EXPECT_EQ("a", config_.video_capabilities[0].content_type);
  EXPECT_EQ("b", config_.video_capabilities[1].content_type);
}

TEST_F(KeySystemConfigSelectorTest,
       VideoCapabilities_Codecs_SubsetSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].content_type = "a";
  video_capabilities[0].mime_type = kSupportedVideoContainer;
  video_capabilities[0].codecs = kUnsupportedCodecs;

  auto config = EmptyConfiguration();
  config.video_capabilities = video_capabilities;
  configs_.push_back(config);

  SelectConfigReturnsError();
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Codecs_AllSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].content_type = "a";
  video_capabilities[0].mime_type = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodecs;

  auto config = EmptyConfiguration();
  config.video_capabilities = video_capabilities;
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  ASSERT_EQ(1u, config_.video_capabilities.size());
  EXPECT_EQ(kSupportedVideoCodecs, config_.video_capabilities[0].codecs);
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Missing_Codecs) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].content_type = "a";
  video_capabilities[0].mime_type = kSupportedVideoContainer;

  auto config = EmptyConfiguration();
  config.video_capabilities = video_capabilities;
  configs_.push_back(config);

  SelectConfigReturnsError();
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Robustness_Empty) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].content_type = "a";
  video_capabilities[0].mime_type = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodec;
  ASSERT_TRUE(video_capabilities[0].robustness.IsEmpty());

  auto config = EmptyConfiguration();
  config.video_capabilities = video_capabilities;
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  ASSERT_EQ(1u, config_.video_capabilities.size());
  EXPECT_TRUE(config_.video_capabilities[0].robustness.IsEmpty());
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Robustness_Supported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].content_type = "a";
  video_capabilities[0].mime_type = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodec;
  video_capabilities[0].robustness = kSupportedRobustness;

  auto config = EmptyConfiguration();
  config.video_capabilities = video_capabilities;
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  ASSERT_EQ(1u, config_.video_capabilities.size());
  EXPECT_EQ(kSupportedRobustness, config_.video_capabilities[0].robustness);
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Robustness_Unsupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].content_type = "a";
  video_capabilities[0].mime_type = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodec;
  video_capabilities[0].robustness = kUnsupportedRobustness;

  auto config = EmptyConfiguration();
  config.video_capabilities = video_capabilities;
  configs_.push_back(config);

  SelectConfigReturnsError();
}

TEST_F(KeySystemConfigSelectorTest,
       VideoCapabilities_Robustness_PermissionCanBeRequired) {
  media_permission_->is_granted = true;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].content_type = "a";
  video_capabilities[0].mime_type = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodec;
  video_capabilities[0].robustness = kRequireIdentifierRobustness;

  auto config = EmptyConfiguration();
  config.video_capabilities = video_capabilities;
  configs_.push_back(config);

  SelectConfigRequestsPermissionAndReturnsConfig();
  EXPECT_EQ(MediaKeysRequirement::kRequired, config_.distinctive_identifier);
}

TEST_F(KeySystemConfigSelectorTest,
       VideoCapabilities_Robustness_PermissionCanBeRecommended) {
  media_permission_->is_granted = false;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].content_type = "a";
  video_capabilities[0].mime_type = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodec;
  video_capabilities[0].robustness = kRecommendIdentifierRobustness;

  auto config = EmptyConfiguration();
  config.video_capabilities = video_capabilities;
  configs_.push_back(config);

  SelectConfigRequestsPermissionAndReturnsConfig();
  EXPECT_EQ(MediaKeysRequirement::kNotAllowed, config_.distinctive_identifier);
}

// --- audioCapabilities ---
// These are handled by the same code as |videoCapabilities|, so only minimal
// additional testing is done.

TEST_F(KeySystemConfigSelectorTest, AudioCapabilities_SubsetSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> audio_capabilities(2);
  audio_capabilities[0].content_type = "a";
  audio_capabilities[0].mime_type = kUnsupportedContainer;
  audio_capabilities[1].content_type = "b";
  audio_capabilities[1].mime_type = kSupportedAudioContainer;
  audio_capabilities[1].codecs = kSupportedAudioCodec;

  auto config = EmptyConfiguration();
  config.audio_capabilities = audio_capabilities;
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  ASSERT_EQ(1u, config_.audio_capabilities.size());
  EXPECT_EQ("b", config_.audio_capabilities[0].content_type);
  EXPECT_EQ(kSupportedAudioContainer, config_.audio_capabilities[0].mime_type);
}

// --- Multiple configurations ---

TEST_F(KeySystemConfigSelectorTest, Configurations_AllSupported) {
  auto config = UsableConfiguration();
  config.label = "a";
  configs_.push_back(config);
  config.label = "b";
  configs_.push_back(config);

  SelectConfigReturnsConfig();
  ASSERT_EQ("a", config_.label);
}

TEST_F(KeySystemConfigSelectorTest, Configurations_SubsetSupported) {
  auto config1 = UsableConfiguration();
  config1.label = "a";
  std::vector<blink::WebEncryptedMediaInitDataType> init_data_types;
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::kUnknown);
  config1.init_data_types = init_data_types;
  configs_.push_back(config1);

  auto config2 = UsableConfiguration();
  config2.label = "b";
  configs_.push_back(config2);

  SelectConfigReturnsConfig();
  ASSERT_EQ("b", config_.label);
}

TEST_F(KeySystemConfigSelectorTest,
       Configurations_FirstRequiresPermission_Allowed) {
  media_permission_->is_granted = true;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  auto config1 = UsableConfiguration();
  config1.label = "a";
  config1.distinctive_identifier = MediaKeysRequirement::kRequired;
  configs_.push_back(config1);

  auto config2 = UsableConfiguration();
  config2.label = "b";
  configs_.push_back(config2);

  SelectConfigRequestsPermissionAndReturnsConfig();
  ASSERT_EQ("a", config_.label);
}

TEST_F(KeySystemConfigSelectorTest,
       Configurations_FirstRequiresPermission_Rejected) {
  media_permission_->is_granted = false;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  auto config1 = UsableConfiguration();
  config1.label = "a";
  config1.distinctive_identifier = MediaKeysRequirement::kRequired;
  configs_.push_back(config1);

  auto config2 = UsableConfiguration();
  config2.label = "b";
  configs_.push_back(config2);

  SelectConfigRequestsPermissionAndReturnsConfig();
  ASSERT_EQ("b", config_.label);
}

}  // namespace media
