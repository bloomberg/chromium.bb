// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_client.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "media/base/media_switches.h"
#include "media/base/video_color_space.h"
#include "ui/base/ui_base_switches.h"

namespace content {

void RenderMediaClient::Initialize() {
  GetInstance();
}

RenderMediaClient::RenderMediaClient()
    : has_updated_(false),
      is_update_needed_(true),
      tick_clock_(new base::DefaultTickClock()) {
  media::SetMediaClient(this);
}

RenderMediaClient::~RenderMediaClient() {
}

void RenderMediaClient::AddKeySystemsInfoForUMA(
    std::vector<media::KeySystemInfoForUMA>* key_systems_info_for_uma) {
  DVLOG(2) << __func__;
#if defined(WIDEVINE_CDM_AVAILABLE)
  key_systems_info_for_uma->push_back(media::KeySystemInfoForUMA(
      kWidevineKeySystem, kWidevineKeySystemNameForUMA));
#endif  // WIDEVINE_CDM_AVAILABLE
}

bool RenderMediaClient::IsKeySystemsUpdateNeeded() {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Always needs update if we have never updated, regardless the
  // |last_update_time_ticks_|'s initial value.
  if (!has_updated_) {
    DCHECK(is_update_needed_);
    return true;
  }

  if (!is_update_needed_)
    return false;

  // The update could be expensive. For example, it could involve a sync IPC to
  // the browser process. Use a minimum update interval to avoid unnecessarily
  // frequent update.
  static const int kMinUpdateIntervalInMilliseconds = 1000;
  if ((tick_clock_->NowTicks() - last_update_time_ticks_).InMilliseconds() <
      kMinUpdateIntervalInMilliseconds) {
    return false;
  }

  return true;
}

void RenderMediaClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<media::KeySystemProperties>>*
        key_systems_properties) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  GetContentClient()->renderer()->AddSupportedKeySystems(
      key_systems_properties);

  has_updated_ = true;
  last_update_time_ticks_ = tick_clock_->NowTicks();

  // Check whether all potentially supported key systems are supported. If so,
  // no need to update again.
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
  for (const auto& properties : *key_systems_properties) {
    if (properties->GetKeySystemName() == kWidevineKeySystem)
      is_update_needed_ = false;
  }
#else
  is_update_needed_ = false;
#endif
}

void RenderMediaClient::RecordRapporURL(const std::string& metric,
                                        const GURL& url) {
  GetContentClient()->renderer()->RecordRapporURL(metric, url);
}

bool IsColorSpaceSupported(const media::VideoColorSpace& color_space) {
  bool color_management =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableHDROutput) ||
      base::FeatureList::IsEnabled(media::kVideoColorManagement);
  switch (color_space.primaries) {
    case media::VideoColorSpace::PrimaryID::EBU_3213_E:
    case media::VideoColorSpace::PrimaryID::INVALID:
      return false;

    // Transfers supported without color management.
    case media::VideoColorSpace::PrimaryID::BT709:
    case media::VideoColorSpace::PrimaryID::UNSPECIFIED:
    case media::VideoColorSpace::PrimaryID::BT470M:
    case media::VideoColorSpace::PrimaryID::BT470BG:
    case media::VideoColorSpace::PrimaryID::SMPTE170M:
      break;

    // Supported with color management.
    case media::VideoColorSpace::PrimaryID::SMPTE240M:
    case media::VideoColorSpace::PrimaryID::FILM:
    case media::VideoColorSpace::PrimaryID::BT2020:
    case media::VideoColorSpace::PrimaryID::SMPTEST428_1:
    case media::VideoColorSpace::PrimaryID::SMPTEST431_2:
    case media::VideoColorSpace::PrimaryID::SMPTEST432_1:
      if (!color_management)
        return false;
      break;
  }

  switch (color_space.transfer) {
    // Transfers supported without color management.
    case media::VideoColorSpace::TransferID::UNSPECIFIED:
    case media::VideoColorSpace::TransferID::GAMMA22:
    case media::VideoColorSpace::TransferID::BT709:
    case media::VideoColorSpace::TransferID::SMPTE170M:
    case media::VideoColorSpace::TransferID::BT2020_10:
    case media::VideoColorSpace::TransferID::BT2020_12:
    case media::VideoColorSpace::TransferID::IEC61966_2_1:
      break;

    // Supported with color management.
    case media::VideoColorSpace::TransferID::GAMMA28:
    case media::VideoColorSpace::TransferID::SMPTE240M:
    case media::VideoColorSpace::TransferID::LINEAR:
    case media::VideoColorSpace::TransferID::LOG:
    case media::VideoColorSpace::TransferID::LOG_SQRT:
    case media::VideoColorSpace::TransferID::BT1361_ECG:
    case media::VideoColorSpace::TransferID::SMPTEST2084:
    case media::VideoColorSpace::TransferID::IEC61966_2_4:
    case media::VideoColorSpace::TransferID::SMPTEST428_1:
    case media::VideoColorSpace::TransferID::ARIB_STD_B67:
      if (!color_management)
        return false;
      break;

    // Never supported.
    case media::VideoColorSpace::TransferID::INVALID:
      return false;
  }

  switch (color_space.matrix) {
    // Supported without color management.
    case media::VideoColorSpace::MatrixID::BT709:
    case media::VideoColorSpace::MatrixID::UNSPECIFIED:
    case media::VideoColorSpace::MatrixID::BT470BG:
    case media::VideoColorSpace::MatrixID::SMPTE170M:
    case media::VideoColorSpace::MatrixID::BT2020_NCL:
      break;

    // Supported with color management.
    case media::VideoColorSpace::MatrixID::RGB:
    case media::VideoColorSpace::MatrixID::FCC:
    case media::VideoColorSpace::MatrixID::SMPTE240M:
    case media::VideoColorSpace::MatrixID::YCOCG:
    case media::VideoColorSpace::MatrixID::YDZDX:
    case media::VideoColorSpace::MatrixID::BT2020_CL:
      if (!color_management)
        return false;
      break;

    // Never supported.
    case media::VideoColorSpace::MatrixID::INVALID:
      return false;
  }

  if (color_space.range == gfx::ColorSpace::RangeID::INVALID)
    return false;

  return true;
}

bool RenderMediaClient::IsSupportedVideoConfig(
    const media::VideoConfig& config) {
  // TODO(chcunningham): Query decoders for codec profile support.
  switch (config.codec) {
    case media::kCodecVP9:
      // Color management required for HDR to not look terrible.
      return IsColorSpaceSupported(config.color_space);

    case media::kCodecH264:
    case media::kCodecVP8:
    case media::kCodecTheora:
      return true;

    case media::kUnknownVideoCodec:
    case media::kCodecVC1:
    case media::kCodecMPEG2:
    case media::kCodecMPEG4:
    case media::kCodecHEVC:
    case media::kCodecDolbyVision:
      return false;
  }

  NOTREACHED();
  return false;
}

void RenderMediaClient::SetTickClockForTesting(
    std::unique_ptr<base::TickClock> tick_clock) {
  tick_clock_.swap(tick_clock);
}

// static
RenderMediaClient* RenderMediaClient::GetInstance() {
  static RenderMediaClient* client = new RenderMediaClient();
  return client;
}

}  // namespace content
