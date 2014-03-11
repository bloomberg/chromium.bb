// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/base/android/media_drm_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

namespace media {

#define EXPECT_TRUE_IF_AVAILABLE(a)                   \
  do {                                                \
    if (!MediaDrmBridge::IsAvailable()) {             \
      VLOG(0) << "MediaDrm not supported on device."; \
      EXPECT_FALSE(a);                                \
    } else {                                          \
      EXPECT_TRUE(a);                                 \
    }                                                 \
  } while (0)

const char kAudioMp4[] = "audio/mp4";
const char kVideoMp4[] = "video/mp4";
const char kAudioWebM[] = "audio/webm";
const char kVideoWebM[] = "video/webm";
const char kInvalidKeySystem[] = "invalid.keysystem";
const MediaDrmBridge::SecurityLevel kLNone =
    MediaDrmBridge::SECURITY_LEVEL_NONE;
const MediaDrmBridge::SecurityLevel kL1 = MediaDrmBridge::SECURITY_LEVEL_1;
const MediaDrmBridge::SecurityLevel kL3 = MediaDrmBridge::SECURITY_LEVEL_3;

static bool IsKeySystemSupportedWithType(
    const std::string& key_system,
    const std::string& container_mime_type) {
  return MediaDrmBridge::IsKeySystemSupportedWithType(key_system,
                                                      container_mime_type);
}

static bool IsSecurityLevelSupported(
    const std::string& key_system,
    MediaDrmBridge::SecurityLevel security_level) {
  return MediaDrmBridge::IsSecurityLevelSupported(key_system, security_level);
}

TEST(MediaDrmBridgeTest, IsSecurityLevelSupported_Widevine) {
  EXPECT_FALSE(IsSecurityLevelSupported(kWidevineKeySystem, kLNone));
  // We test "L3" fully. But for "L1" we don't check the result as it depends on
  // whether the test device supports "L1".
  EXPECT_TRUE_IF_AVAILABLE(IsSecurityLevelSupported(kWidevineKeySystem, kL3));
  IsSecurityLevelSupported(kWidevineKeySystem, kL1);
}

// Invalid keysytem is NOT supported regardless whether MediaDrm is available.
TEST(MediaDrmBridgeTest, IsSecurityLevelSupported_InvalidKeySystem) {
  EXPECT_FALSE(IsSecurityLevelSupported(kInvalidKeySystem, kLNone));
  EXPECT_FALSE(IsSecurityLevelSupported(kInvalidKeySystem, kL1));
  EXPECT_FALSE(IsSecurityLevelSupported(kInvalidKeySystem, kL3));
}

TEST(MediaDrmBridgeTest, IsTypeSupported_Widevine) {
  EXPECT_TRUE_IF_AVAILABLE(
      IsKeySystemSupportedWithType(kWidevineKeySystem, kAudioMp4));
  EXPECT_TRUE_IF_AVAILABLE(
      IsKeySystemSupportedWithType(kWidevineKeySystem, kVideoMp4));

  // TODO(xhwang): MediaDrmBridge.IsKeySystemSupportedWithType() doesn't check
  // the container type. Fix IsKeySystemSupportedWithType() and update this test
  // as necessary. See: http://crbug.com/350481
  EXPECT_TRUE_IF_AVAILABLE(
      IsKeySystemSupportedWithType(kWidevineKeySystem, kAudioWebM));
  EXPECT_TRUE_IF_AVAILABLE(
      IsKeySystemSupportedWithType(kWidevineKeySystem, kVideoWebM));
}

// Invalid keysytem is NOT supported regardless whether MediaDrm is available.
TEST(MediaDrmBridgeTest, IsTypeSupported_InvalidKeySystem) {
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, ""));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, kVideoMp4));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, kVideoWebM));
}

}  // namespace media
