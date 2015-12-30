// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/test/simple_test_tick_clock.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/media/render_media_client.h"
#include "content/test/test_content_client.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

namespace content {

namespace {

class TestContentRendererClient : public ContentRendererClient {
 public:
  TestContentRendererClient() : is_extra_key_system_enabled_(false) {}

  // ContentRendererClient implementation.
  void AddKeySystems(
      std::vector<media::KeySystemInfo>* key_systems_info) override {
    // TODO(sandersd): Was this supposed to be added to the list?
    media::KeySystemInfo key_system_info;
    key_system_info.key_system = "test.keysystem";
    key_system_info.max_audio_robustness = media::EmeRobustness::EMPTY;
    key_system_info.max_video_robustness = media::EmeRobustness::EMPTY;
    key_system_info.persistent_license_support =
        media::EmeSessionTypeSupport::NOT_SUPPORTED;
    key_system_info.persistent_release_message_support =
        media::EmeSessionTypeSupport::NOT_SUPPORTED;
    key_system_info.persistent_state_support =
        media::EmeFeatureSupport::NOT_SUPPORTED;
    key_system_info.distinctive_identifier_support =
        media::EmeFeatureSupport::NOT_SUPPORTED;
    key_systems_info->push_back(key_system_info);
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
    if (is_extra_key_system_enabled_) {
      media::KeySystemInfo wv_key_system_info;
      wv_key_system_info.key_system = kWidevineKeySystem;
      wv_key_system_info.max_audio_robustness = media::EmeRobustness::EMPTY;
      wv_key_system_info.max_video_robustness = media::EmeRobustness::EMPTY;
      wv_key_system_info.persistent_license_support =
          media::EmeSessionTypeSupport::NOT_SUPPORTED;
      wv_key_system_info.persistent_release_message_support =
          media::EmeSessionTypeSupport::NOT_SUPPORTED;
      wv_key_system_info.persistent_state_support =
          media::EmeFeatureSupport::NOT_SUPPORTED;
      wv_key_system_info.distinctive_identifier_support =
          media::EmeFeatureSupport::NOT_SUPPORTED;
      key_systems_info->push_back(wv_key_system_info);
    }
#endif
  }

  void EnableExtraKeySystem() { is_extra_key_system_enabled_ = true; }

 private:
  // Whether a platform-specific extra key system is "supported" by |this|.
  bool is_extra_key_system_enabled_;
};

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
bool ContainsWidevine(
    const std::vector<media::KeySystemInfo>& key_systems_info) {
  for (const auto& key_system_info : key_systems_info) {
    if (key_system_info.key_system == kWidevineKeySystem)
      return true;
  }
  return false;
}
#endif

}  // namespace

class RenderMediaClientTest : public testing::Test {
 protected:
  RenderMediaClientTest()
      : render_media_client_(GetRenderMediaClientInstanceForTesting()) {
    SetContentClient(&test_content_client_);
    SetRendererClientForTesting(&test_content_renderer_client_);
  }

  void EnableExtraKeySystem() {
    test_content_renderer_client_.EnableExtraKeySystem();
  }

  RenderMediaClient* render_media_client_;

 private:
  typedef base::hash_map<std::string, std::string> KeySystemNameForUMAMap;

  TestContentClient test_content_client_;
  TestContentRendererClient test_content_renderer_client_;
  KeySystemNameForUMAMap key_system_name_for_uma_map_;
};

TEST_F(RenderMediaClientTest, KeySystemNameForUMA) {
  std::vector<media::KeySystemInfoForUMA> key_systems_info_for_uma;
  render_media_client_->AddKeySystemsInfoForUMA(&key_systems_info_for_uma);

  std::string widevine_uma_name;
  std::string clearkey_uma_name;
  for (const media::KeySystemInfoForUMA& info : key_systems_info_for_uma) {
    if (info.key_system == "com.widevine.alpha")
      widevine_uma_name = info.key_system_name_for_uma;
    if (info.key_system == "org.w3.clearkey")
      clearkey_uma_name = info.key_system_name_for_uma;
  }

#if defined(WIDEVINE_CDM_AVAILABLE)
  EXPECT_EQ("Widevine", widevine_uma_name);
#else
  EXPECT_TRUE(widevine_uma_name.empty());
#endif

  EXPECT_TRUE(clearkey_uma_name.empty()) << "Clear Key is added by media/ and "
                                            "should not be added by the "
                                            "MediaClient.";
}

TEST_F(RenderMediaClientTest, IsKeySystemsUpdateNeeded) {
  base::SimpleTestTickClock* tick_clock = new base::SimpleTestTickClock();
  render_media_client_->SetTickClockForTesting(
      scoped_ptr<base::TickClock>(tick_clock));

  // IsKeySystemsUpdateNeeded() always returns true after construction.
  EXPECT_TRUE(render_media_client_->IsKeySystemsUpdateNeeded());

  std::vector<media::KeySystemInfo> key_systems_info;
  render_media_client_->AddSupportedKeySystems(&key_systems_info);

  // No update needed immediately after AddSupportedKeySystems() call.
  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
  // Widevine not supported because extra key system isn't enabled.
  EXPECT_FALSE(ContainsWidevine(key_systems_info));

  // This is timing related. The update interval for Widevine is 1000 ms.
  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());
  tick_clock->Advance(base::TimeDelta::FromMilliseconds(990));
  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());
  tick_clock->Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(render_media_client_->IsKeySystemsUpdateNeeded());

  EnableExtraKeySystem();

  key_systems_info.clear();
  render_media_client_->AddSupportedKeySystems(&key_systems_info);
  EXPECT_TRUE(ContainsWidevine(key_systems_info));

  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());
  tick_clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());
  tick_clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());
#endif
}

}  // namespace content
