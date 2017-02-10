// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/base/eme_constants.h"
#include "media/base/key_systems.h"
#include "media/base/media_permission.h"
#include "media/blink/key_system_config_selector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaTypes.h"
#include "third_party/WebKit/public/platform/WebMediaKeySystemConfiguration.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "url/gurl.h"

namespace media {

namespace {

const char kSupported[] = "supported";
const char kRecommendIdentifier[] = "recommend_identifier";
const char kRequireIdentifier[] = "require_identifier";
const char kUnsupported[] = "unsupported";

const char kSupportedVideoContainer[] = "video/webm";
const char kSupportedAudioContainer[] = "audio/webm";
const char kUnsupportedContainer[] = "video/foo";

// TODO(sandersd): Extended codec variants (requires proprietary codec support).
const char kSupportedVideoCodec[] = "vp8";
const char kSupportedAudioCodec[] = "opus";
const char kUnsupportedCodec[] = "foo";
const char kUnsupportedCodecs[] = "vp8,foo";
const char kSupportedVideoCodecs[] = "vp8,vp8";

const char kDefaultSecurityOrigin[] = "https://example.com/";

// The IDL for MediaKeySystemConfiguration specifies some defaults, so
// create a config object that mimics what would be created if an empty
// dictionary was passed in.
blink::WebMediaKeySystemConfiguration EmptyConfiguration() {
  // http://w3c.github.io/encrypted-media/#mediakeysystemconfiguration-dictionary
  // If this member (sessionTypes) is not present when the dictionary
  // is passed to requestMediaKeySystemAccess(), the dictionary will
  // be treated as if this member is set to [ "temporary" ].
  std::vector<blink::WebEncryptedMediaSessionType> session_types;
  session_types.push_back(blink::WebEncryptedMediaSessionType::Temporary);

  blink::WebMediaKeySystemConfiguration config;
  config.label = "";
  config.sessionTypes = session_types;
  return config;
}

// EME spec requires that at least one of |video_capabilities| and
// |audio_capabilities| be specified. Add a single valid audio capability
// to the EmptyConfiguration().
blink::WebMediaKeySystemConfiguration UsableConfiguration() {
  // Blink code parses the contentType into mimeType and codecs, so mimic
  // that here.
  std::vector<blink::WebMediaKeySystemMediaCapability> audio_capabilities(1);
  audio_capabilities[0].mimeType = kSupportedAudioContainer;
  audio_capabilities[0].codecs = kSupportedAudioCodec;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.audioCapabilities = audio_capabilities;
  return config;
}

class FakeKeySystems : public KeySystems {
 public:
  ~FakeKeySystems() override {
  }

  bool IsSupportedKeySystem(const std::string& key_system) const override {
    if (key_system == kSupported)
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

  // TODO(sandersd): Secure codec simulation.
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
    if (requested_robustness == kUnsupported)
      return EmeConfigRule::NOT_SUPPORTED;
    if (requested_robustness == kRequireIdentifier)
      return EmeConfigRule::IDENTIFIER_REQUIRED;
    if (requested_robustness == kRecommendIdentifier)
      return EmeConfigRule::IDENTIFIER_RECOMMENDED;
    if (requested_robustness == kSupported)
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
  void HasPermission(Type type,
                     const GURL& security_origin,
                     const PermissionStatusCB& permission_status_cb) override {
    permission_status_cb.Run(is_granted);
  }

  void RequestPermission(
      Type type,
      const GURL& security_origin,
      const PermissionStatusCB& permission_status_cb) override {
    requests++;
    permission_status_cb.Run(is_granted);
  }

  int requests = 0;
  bool is_granted = false;
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
        .SelectConfig(key_system_, configs_, security_origin_, false,
                      base::Bind(&KeySystemConfigSelectorTest::OnSucceeded,
                                 base::Unretained(this)),
                      base::Bind(&KeySystemConfigSelectorTest::OnNotSupported,
                                 base::Unretained(this)));
  }

  bool SelectConfigReturnsConfig() {
    SelectConfig();
    EXPECT_EQ(0, media_permission_->requests);
    EXPECT_EQ(1, succeeded_count_);
    EXPECT_EQ(0, not_supported_count_);
    return (succeeded_count_ != 0);
  }

  bool SelectConfigReturnsError() {
    SelectConfig();
    EXPECT_EQ(0, media_permission_->requests);
    EXPECT_EQ(0, succeeded_count_);
    EXPECT_EQ(1, not_supported_count_);
    return (not_supported_count_ != 0);
  }

  bool SelectConfigRequestsPermissionAndReturnsConfig() {
    SelectConfig();
    EXPECT_EQ(1, media_permission_->requests);
    EXPECT_EQ(1, succeeded_count_);
    EXPECT_EQ(0, not_supported_count_);
    return (media_permission_->requests != 0 && succeeded_count_ != 0);
  }

  bool SelectConfigRequestsPermissionAndReturnsError() {
    SelectConfig();
    EXPECT_EQ(1, media_permission_->requests);
    EXPECT_EQ(0, succeeded_count_);
    EXPECT_EQ(1, not_supported_count_);
    return (media_permission_->requests != 0 && not_supported_count_ != 0);
  }

  void OnSucceeded(const blink::WebMediaKeySystemConfiguration& result,
                   const CdmConfig& cdm_config) {
    succeeded_count_++;
    config_ = result;
  }

  void OnNotSupported(const blink::WebString&) { not_supported_count_++; }

  std::unique_ptr<FakeKeySystems> key_systems_;
  std::unique_ptr<FakeMediaPermission> media_permission_;

  // Held values for the call to SelectConfig().
  blink::WebString key_system_ = blink::WebString::fromUTF8(kSupported);
  std::vector<blink::WebMediaKeySystemConfiguration> configs_;
  blink::WebSecurityOrigin security_origin_ =
      blink::WebSecurityOrigin::createFromString(kDefaultSecurityOrigin);

  // Holds the last successful accumulated configuration.
  blink::WebMediaKeySystemConfiguration config_;

  int succeeded_count_;
  int not_supported_count_;

  DISALLOW_COPY_AND_ASSIGN(KeySystemConfigSelectorTest);
};

// --- Basics ---

TEST_F(KeySystemConfigSelectorTest, NoConfigs) {
  ASSERT_TRUE(SelectConfigReturnsError());
}

TEST_F(KeySystemConfigSelectorTest, DefaultConfig) {
  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();

  // label = "";
  ASSERT_EQ("", config.label);

  // initDataTypes = [];
  ASSERT_EQ(0u, config.initDataTypes.size());

  // audioCapabilities = [];
  ASSERT_EQ(0u, config.audioCapabilities.size());

  // videoCapabilities = [];
  ASSERT_EQ(0u, config.videoCapabilities.size());

  // distinctiveIdentifier = "optional";
  ASSERT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::Optional,
            config.distinctiveIdentifier);

  // persistentState = "optional";
  ASSERT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::Optional,
            config.persistentState);

  // If this member is not present when the dictionary is passed to
  // requestMediaKeySystemAccess(), the dictionary will be treated as
  // if this member is set to [ "temporary" ].
  ASSERT_EQ(1u, config.sessionTypes.size());
  ASSERT_EQ(blink::WebEncryptedMediaSessionType::Temporary,
            config.sessionTypes[0]);
}

TEST_F(KeySystemConfigSelectorTest, EmptyConfig) {
  // EME spec requires that at least one of |video_capabilities| and
  // |audio_capabilities| be specified.
  configs_.push_back(EmptyConfiguration());
  ASSERT_TRUE(SelectConfigReturnsError());
}

// Most of the tests below assume that the the usable config is valid.
// Tests that touch |video_capabilities| and/or |audio_capabilities| can
// modify the empty config.

TEST_F(KeySystemConfigSelectorTest, UsableConfig) {
  configs_.push_back(UsableConfiguration());

  ASSERT_TRUE(SelectConfigReturnsConfig());
  EXPECT_EQ("", config_.label);
  EXPECT_TRUE(config_.initDataTypes.isEmpty());
  EXPECT_EQ(1u, config_.audioCapabilities.size());
  EXPECT_TRUE(config_.videoCapabilities.isEmpty());
  EXPECT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed,
            config_.distinctiveIdentifier);
  EXPECT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed,
            config_.persistentState);
  ASSERT_EQ(1u, config_.sessionTypes.size());
  EXPECT_EQ(blink::WebEncryptedMediaSessionType::Temporary,
            config_.sessionTypes[0]);
}

TEST_F(KeySystemConfigSelectorTest, Label) {
  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.label = "foo";
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  EXPECT_EQ("foo", config_.label);
}

// --- keySystem ---
// Empty is not tested because the empty check is in Blink.

TEST_F(KeySystemConfigSelectorTest, KeySystem_NonAscii) {
  key_system_ = "\xde\xad\xbe\xef";
  configs_.push_back(UsableConfiguration());
  ASSERT_TRUE(SelectConfigReturnsError());
}

TEST_F(KeySystemConfigSelectorTest, KeySystem_Unsupported) {
  key_system_ = kUnsupported;
  configs_.push_back(UsableConfiguration());
  ASSERT_TRUE(SelectConfigReturnsError());
}

// --- initDataTypes ---

TEST_F(KeySystemConfigSelectorTest, InitDataTypes_Empty) {
  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
}

TEST_F(KeySystemConfigSelectorTest, InitDataTypes_NoneSupported) {
  key_systems_->init_data_type_webm_supported_ = true;

  std::vector<blink::WebEncryptedMediaInitDataType> init_data_types;
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::Unknown);
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::Cenc);

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.initDataTypes = init_data_types;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsError());
}

TEST_F(KeySystemConfigSelectorTest, InitDataTypes_SubsetSupported) {
  key_systems_->init_data_type_webm_supported_ = true;

  std::vector<blink::WebEncryptedMediaInitDataType> init_data_types;
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::Unknown);
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::Cenc);
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::Webm);

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.initDataTypes = init_data_types;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  ASSERT_EQ(1u, config_.initDataTypes.size());
  EXPECT_EQ(blink::WebEncryptedMediaInitDataType::Webm,
            config_.initDataTypes[0]);
}

// --- distinctiveIdentifier ---

TEST_F(KeySystemConfigSelectorTest, DistinctiveIdentifier_Default) {
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.distinctiveIdentifier =
      blink::WebMediaKeySystemConfiguration::Requirement::Optional;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  EXPECT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed,
            config_.distinctiveIdentifier);
}

TEST_F(KeySystemConfigSelectorTest, DistinctiveIdentifier_Forced) {
  media_permission_->is_granted = true;
  key_systems_->distinctive_identifier = EmeFeatureSupport::ALWAYS_ENABLED;

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.distinctiveIdentifier =
      blink::WebMediaKeySystemConfiguration::Requirement::Optional;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigRequestsPermissionAndReturnsConfig());
  EXPECT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::Required,
            config_.distinctiveIdentifier);
}

TEST_F(KeySystemConfigSelectorTest, DistinctiveIdentifier_Blocked) {
  key_systems_->distinctive_identifier = EmeFeatureSupport::NOT_SUPPORTED;

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.distinctiveIdentifier =
      blink::WebMediaKeySystemConfiguration::Requirement::Required;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsError());
}

TEST_F(KeySystemConfigSelectorTest, DistinctiveIdentifier_RequestsPermission) {
  media_permission_->is_granted = true;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.distinctiveIdentifier =
      blink::WebMediaKeySystemConfiguration::Requirement::Required;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigRequestsPermissionAndReturnsConfig());
  EXPECT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::Required,
            config_.distinctiveIdentifier);
}

TEST_F(KeySystemConfigSelectorTest, DistinctiveIdentifier_RespectsPermission) {
  media_permission_->is_granted = false;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.distinctiveIdentifier =
      blink::WebMediaKeySystemConfiguration::Requirement::Required;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigRequestsPermissionAndReturnsError());
}

// --- persistentState ---

TEST_F(KeySystemConfigSelectorTest, PersistentState_Default) {
  key_systems_->persistent_state = EmeFeatureSupport::REQUESTABLE;

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.persistentState =
      blink::WebMediaKeySystemConfiguration::Requirement::Optional;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  EXPECT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed,
            config_.persistentState);
}

TEST_F(KeySystemConfigSelectorTest, PersistentState_Forced) {
  key_systems_->persistent_state = EmeFeatureSupport::ALWAYS_ENABLED;

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.persistentState =
      blink::WebMediaKeySystemConfiguration::Requirement::Optional;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  EXPECT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::Required,
            config_.persistentState);
}

TEST_F(KeySystemConfigSelectorTest, PersistentState_Blocked) {
  key_systems_->persistent_state = EmeFeatureSupport::ALWAYS_ENABLED;

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.persistentState =
      blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsError());
}

// --- sessionTypes ---

TEST_F(KeySystemConfigSelectorTest, SessionTypes_Empty) {
  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();

  // Usable configuration has [ "temporary" ].
  std::vector<blink::WebEncryptedMediaSessionType> session_types;
  config.sessionTypes = session_types;

  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  EXPECT_TRUE(config_.sessionTypes.isEmpty());
}

TEST_F(KeySystemConfigSelectorTest, SessionTypes_SubsetSupported) {
  // Allow persistent state, as it would be required to be successful.
  key_systems_->persistent_state = EmeFeatureSupport::REQUESTABLE;
  key_systems_->persistent_license = EmeSessionTypeSupport::NOT_SUPPORTED;

  std::vector<blink::WebEncryptedMediaSessionType> session_types;
  session_types.push_back(blink::WebEncryptedMediaSessionType::Temporary);
  session_types.push_back(
      blink::WebEncryptedMediaSessionType::PersistentLicense);

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.sessionTypes = session_types;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsError());
}

TEST_F(KeySystemConfigSelectorTest, SessionTypes_AllSupported) {
  // Allow persistent state, and expect it to be required.
  key_systems_->persistent_state = EmeFeatureSupport::REQUESTABLE;
  key_systems_->persistent_license = EmeSessionTypeSupport::SUPPORTED;

  std::vector<blink::WebEncryptedMediaSessionType> session_types;
  session_types.push_back(blink::WebEncryptedMediaSessionType::Temporary);
  session_types.push_back(
      blink::WebEncryptedMediaSessionType::PersistentLicense);

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.persistentState =
      blink::WebMediaKeySystemConfiguration::Requirement::Optional;
  config.sessionTypes = session_types;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  EXPECT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::Required,
            config_.persistentState);
  ASSERT_EQ(2u, config_.sessionTypes.size());
  EXPECT_EQ(blink::WebEncryptedMediaSessionType::Temporary,
            config_.sessionTypes[0]);
  EXPECT_EQ(blink::WebEncryptedMediaSessionType::PersistentLicense,
            config_.sessionTypes[1]);
}

TEST_F(KeySystemConfigSelectorTest, SessionTypes_PermissionCanBeRequired) {
  media_permission_->is_granted = true;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;
  key_systems_->persistent_state = EmeFeatureSupport::REQUESTABLE;
  key_systems_->persistent_license =
      EmeSessionTypeSupport::SUPPORTED_WITH_IDENTIFIER;

  std::vector<blink::WebEncryptedMediaSessionType> session_types;
  session_types.push_back(
      blink::WebEncryptedMediaSessionType::PersistentLicense);

  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.distinctiveIdentifier =
      blink::WebMediaKeySystemConfiguration::Requirement::Optional;
  config.persistentState =
      blink::WebMediaKeySystemConfiguration::Requirement::Optional;
  config.sessionTypes = session_types;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigRequestsPermissionAndReturnsConfig());
  EXPECT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::Required,
            config_.distinctiveIdentifier);
}

// --- videoCapabilities ---

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Empty) {
  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_NoneSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(2);
  video_capabilities[0].contentType = "a";
  video_capabilities[0].mimeType = kUnsupportedContainer;
  video_capabilities[1].contentType = "b";
  video_capabilities[1].mimeType = kSupportedVideoContainer;
  video_capabilities[1].codecs = kUnsupportedCodec;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.videoCapabilities = video_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsError());
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_SubsetSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(2);
  video_capabilities[0].contentType = "a";
  video_capabilities[0].mimeType = kUnsupportedContainer;
  video_capabilities[1].contentType = "b";
  video_capabilities[1].mimeType = kSupportedVideoContainer;
  video_capabilities[1].codecs = kSupportedVideoCodec;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.videoCapabilities = video_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  ASSERT_EQ(1u, config_.videoCapabilities.size());
  EXPECT_EQ("b", config_.videoCapabilities[0].contentType);
  EXPECT_EQ(kSupportedVideoContainer, config_.videoCapabilities[0].mimeType);
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_AllSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(2);
  video_capabilities[0].contentType = "a";
  video_capabilities[0].mimeType = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodecs;
  video_capabilities[1].contentType = "b";
  video_capabilities[1].mimeType = kSupportedVideoContainer;
  video_capabilities[1].codecs = kSupportedVideoCodecs;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.videoCapabilities = video_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  ASSERT_EQ(2u, config_.videoCapabilities.size());
  EXPECT_EQ("a", config_.videoCapabilities[0].contentType);
  EXPECT_EQ("b", config_.videoCapabilities[1].contentType);
}

TEST_F(KeySystemConfigSelectorTest,
       VideoCapabilities_Codecs_SubsetSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].contentType = "a";
  video_capabilities[0].mimeType = kSupportedVideoContainer;
  video_capabilities[0].codecs = kUnsupportedCodecs;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.videoCapabilities = video_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsError());
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Codecs_AllSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].contentType = "a";
  video_capabilities[0].mimeType = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodecs;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.videoCapabilities = video_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  ASSERT_EQ(1u, config_.videoCapabilities.size());
  EXPECT_EQ(kSupportedVideoCodecs, config_.videoCapabilities[0].codecs);
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Missing_Codecs) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].contentType = "a";
  video_capabilities[0].mimeType = kSupportedVideoContainer;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.videoCapabilities = video_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsError());
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Robustness_Empty) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].contentType = "a";
  video_capabilities[0].mimeType = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodec;
  ASSERT_TRUE(video_capabilities[0].robustness.isEmpty());

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.videoCapabilities = video_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  ASSERT_EQ(1u, config_.videoCapabilities.size());
  EXPECT_TRUE(config_.videoCapabilities[0].robustness.isEmpty());
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Robustness_Supported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].contentType = "a";
  video_capabilities[0].mimeType = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodec;
  video_capabilities[0].robustness = kSupported;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.videoCapabilities = video_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  ASSERT_EQ(1u, config_.videoCapabilities.size());
  EXPECT_EQ(kSupported, config_.videoCapabilities[0].robustness);
}

TEST_F(KeySystemConfigSelectorTest, VideoCapabilities_Robustness_Unsupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].contentType = "a";
  video_capabilities[0].mimeType = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodec;
  video_capabilities[0].robustness = kUnsupported;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.videoCapabilities = video_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsError());
}

TEST_F(KeySystemConfigSelectorTest,
       VideoCapabilities_Robustness_PermissionCanBeRequired) {
  media_permission_->is_granted = true;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].contentType = "a";
  video_capabilities[0].mimeType = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodec;
  video_capabilities[0].robustness = kRequireIdentifier;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.videoCapabilities = video_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigRequestsPermissionAndReturnsConfig());
  EXPECT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::Required,
            config_.distinctiveIdentifier);
}

TEST_F(KeySystemConfigSelectorTest,
       VideoCapabilities_Robustness_PermissionCanBeRecommended) {
  media_permission_->is_granted = false;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities(1);
  video_capabilities[0].contentType = "a";
  video_capabilities[0].mimeType = kSupportedVideoContainer;
  video_capabilities[0].codecs = kSupportedVideoCodec;
  video_capabilities[0].robustness = kRecommendIdentifier;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.videoCapabilities = video_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigRequestsPermissionAndReturnsConfig());
  EXPECT_EQ(blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed,
            config_.distinctiveIdentifier);
}

// --- audioCapabilities ---
// These are handled by the same code as |videoCapabilities|, so only minimal
// additional testing is done.

TEST_F(KeySystemConfigSelectorTest, AudioCapabilities_SubsetSupported) {
  std::vector<blink::WebMediaKeySystemMediaCapability> audio_capabilities(2);
  audio_capabilities[0].contentType = "a";
  audio_capabilities[0].mimeType = kUnsupportedContainer;
  audio_capabilities[1].contentType = "b";
  audio_capabilities[1].mimeType = kSupportedAudioContainer;
  audio_capabilities[1].codecs = kSupportedAudioCodec;

  blink::WebMediaKeySystemConfiguration config = EmptyConfiguration();
  config.audioCapabilities = audio_capabilities;
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  ASSERT_EQ(1u, config_.audioCapabilities.size());
  EXPECT_EQ("b", config_.audioCapabilities[0].contentType);
  EXPECT_EQ(kSupportedAudioContainer, config_.audioCapabilities[0].mimeType);
}

// --- Multiple configurations ---

TEST_F(KeySystemConfigSelectorTest, Configurations_AllSupported) {
  blink::WebMediaKeySystemConfiguration config = UsableConfiguration();
  config.label = "a";
  configs_.push_back(config);
  config.label = "b";
  configs_.push_back(config);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  ASSERT_EQ("a", config_.label);
}

TEST_F(KeySystemConfigSelectorTest, Configurations_SubsetSupported) {
  blink::WebMediaKeySystemConfiguration config1 = UsableConfiguration();
  config1.label = "a";
  std::vector<blink::WebEncryptedMediaInitDataType> init_data_types;
  init_data_types.push_back(blink::WebEncryptedMediaInitDataType::Unknown);
  config1.initDataTypes = init_data_types;
  configs_.push_back(config1);

  blink::WebMediaKeySystemConfiguration config2 = UsableConfiguration();
  config2.label = "b";
  configs_.push_back(config2);

  ASSERT_TRUE(SelectConfigReturnsConfig());
  ASSERT_EQ("b", config_.label);
}

TEST_F(KeySystemConfigSelectorTest,
       Configurations_FirstRequiresPermission_Allowed) {
  media_permission_->is_granted = true;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  blink::WebMediaKeySystemConfiguration config1 = UsableConfiguration();
  config1.label = "a";
  config1.distinctiveIdentifier =
      blink::WebMediaKeySystemConfiguration::Requirement::Required;
  configs_.push_back(config1);

  blink::WebMediaKeySystemConfiguration config2 = UsableConfiguration();
  config2.label = "b";
  configs_.push_back(config2);

  ASSERT_TRUE(SelectConfigRequestsPermissionAndReturnsConfig());
  ASSERT_EQ("a", config_.label);
}

TEST_F(KeySystemConfigSelectorTest,
       Configurations_FirstRequiresPermission_Rejected) {
  media_permission_->is_granted = false;
  key_systems_->distinctive_identifier = EmeFeatureSupport::REQUESTABLE;

  blink::WebMediaKeySystemConfiguration config1 = UsableConfiguration();
  config1.label = "a";
  config1.distinctiveIdentifier =
      blink::WebMediaKeySystemConfiguration::Requirement::Required;
  configs_.push_back(config1);

  blink::WebMediaKeySystemConfiguration config2 = UsableConfiguration();
  config2.label = "b";
  configs_.push_back(config2);

  ASSERT_TRUE(SelectConfigRequestsPermissionAndReturnsConfig());
  ASSERT_EQ("b", config_.label);
}

}  // namespace media
