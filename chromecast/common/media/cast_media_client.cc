// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/media/cast_media_client.h"

#include "base/lazy_instance.h"
#include "chromecast/media/base/media_codec_support.h"
#include "chromecast/public/media/media_capabilities_shlib.h"

// TODO(servolk): Is there a better way to override just IsSupportedVideoConfig
// without duplicating content::RenderMediaClient implementation?
// For now use this to allow access to the ::media::GetMediaClient.
namespace media {
MEDIA_EXPORT MediaClient* GetMediaClient();
}

namespace chromecast {
namespace media {

static base::LazyInstance<CastMediaClient>::Leaky g_cast_media_client =
    LAZY_INSTANCE_INITIALIZER;

void CastMediaClient::Initialize() {
  g_cast_media_client.Get();
}

CastMediaClient::CastMediaClient() {
  content_media_client_ = ::media::GetMediaClient();
  // Ensure that CastMediaClient gets initialized after the
  // content::RenderMediaClient.
  DCHECK(content_media_client_);
  ::media::SetMediaClient(this);
}

CastMediaClient::~CastMediaClient() {}

void CastMediaClient::AddKeySystemsInfoForUMA(
    std::vector<::media::KeySystemInfoForUMA>* key_systems_info_for_uma) {
  content_media_client_->AddKeySystemsInfoForUMA(key_systems_info_for_uma);
}

bool CastMediaClient::IsKeySystemsUpdateNeeded() {
  return content_media_client_->IsKeySystemsUpdateNeeded();
}

void CastMediaClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>*
        key_systems_properties) {
  content_media_client_->AddSupportedKeySystems(key_systems_properties);
}

void CastMediaClient::RecordRapporURL(const std::string& metric,
                                      const GURL& url) {
  content_media_client_->RecordRapporURL(metric, url);
}

// static
bool CastMediaClient::IsSupportedVideoConfig(::media::VideoCodec codec,
                                             ::media::VideoCodecProfile profile,
                                             int level) {
  return MediaCapabilitiesShlib::IsSupportedVideoConfig(
      ToCastVideoCodec(codec), ToCastVideoProfile(profile), level);
}

}  // namespace media
}  // namespace chromecast
