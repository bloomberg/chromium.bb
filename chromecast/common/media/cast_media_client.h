// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_MEDIA_CAST_MEDIA_CLIENT_H_
#define CHROMECAST_COMMON_MEDIA_CAST_MEDIA_CLIENT_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "chromecast/media/base/supported_codec_profile_levels_memo.h"
#include "media/base/media_client.h"

namespace chromecast {
namespace media {

// Forwards MediaClient::IsSupportedVideoConfig calls to MediaCapabilitiesShlib
// and the rest of the calls to the default content media client (i.e.
// content::RenderMediaClient).
class CastMediaClient : public ::media::MediaClient {
 public:
  // Initialize CastMediaClient and SetMediaClient(). Note that the instance
  // is not exposed because no content code needs to directly access it.
  static void Initialize(SupportedCodecProfileLevelsMemo* memo);

  // MediaClient implementation
  void AddKeySystemsInfoForUMA(std::vector<::media::KeySystemInfoForUMA>*
                                   key_systems_info_for_uma) override;
  bool IsKeySystemsUpdateNeeded() override;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<::media::KeySystemProperties>>*
          key_systems_properties) override;
  void RecordRapporURL(const std::string& metric, const GURL& url) override;
  bool IsSupportedVideoConfig(::media::VideoCodec codec,
                              ::media::VideoCodecProfile profile,
                              int level) override;

 private:
  friend struct base::DefaultLazyInstanceTraits<CastMediaClient>;

  CastMediaClient(::media::MediaClient* content_media_client,
                  SupportedCodecProfileLevelsMemo* supported_profiles);
  ~CastMediaClient() override;

  ::media::MediaClient* content_media_client_;
  SupportedCodecProfileLevelsMemo* supported_profiles_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaClient);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_COMMON_MEDIA_CAST_MEDIA_CLIENT_H_
