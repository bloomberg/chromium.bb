// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/media_caps_impl.h"

#include "chromecast/media/base/media_caps.h"

namespace chromecast {
namespace media {

MediaCapsImpl::MediaCapsImpl()
    : supported_codecs_bitmask_(0),
      hdcp_version_(0),
      supported_eotfs_(0),
      dolby_vision_flags_(0),
      current_mode_supports_hdr_(false),
      current_mode_supports_dv_(false),
      screen_resolution_(0, 0) {}

MediaCapsImpl::~MediaCapsImpl() = default;

void MediaCapsImpl::AddBinding(mojom::MediaCapsRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void MediaCapsImpl::SetSupportedHdmiSinkCodecs(
    unsigned int supported_codecs_bitmask) {
  supported_codecs_bitmask_ = supported_codecs_bitmask;

  observers_.ForAllPtrs(
      [supported_codecs_bitmask](mojom::MediaCapsObserver* observer) {
        observer->SupportedHdmiSinkCodecsChanged(supported_codecs_bitmask);
      });
}

void MediaCapsImpl::ScreenResolutionChanged(unsigned width, unsigned height) {
  screen_resolution_ = gfx::Size(width, height);

  observers_.ForAllPtrs([width, height](mojom::MediaCapsObserver* observer) {
    observer->ScreenResolutionChanged(width, height);
  });
}

void MediaCapsImpl::ScreenInfoChanged(int hdcp_version,
                                      int supported_eotfs,
                                      int dolby_vision_flags,
                                      bool current_mode_supports_hdr,
                                      bool current_mode_supports_dv) {
  hdcp_version_ = hdcp_version;
  supported_eotfs_ = supported_eotfs;
  dolby_vision_flags_ = dolby_vision_flags;
  current_mode_supports_hdr_ = current_mode_supports_hdr;
  current_mode_supports_dv_ = current_mode_supports_dv;

  observers_.ForAllPtrs([hdcp_version, supported_eotfs, dolby_vision_flags,
                         current_mode_supports_hdr, current_mode_supports_dv](
      mojom::MediaCapsObserver* observer) {
    observer->ScreenInfoChanged(hdcp_version, supported_eotfs,
                                dolby_vision_flags, current_mode_supports_hdr,
                                current_mode_supports_dv);
  });
}

void MediaCapsImpl::AddObserver(mojom::MediaCapsObserverPtr observer) {
  observer->SupportedHdmiSinkCodecsChanged(supported_codecs_bitmask_);
  observer->ScreenResolutionChanged(screen_resolution_.width(),
                                    screen_resolution_.height());
  observer->ScreenInfoChanged(hdcp_version_, supported_eotfs_,
                              dolby_vision_flags_, current_mode_supports_hdr_,
                              current_mode_supports_dv_);
  observers_.AddPtr(std::move(observer));
}

}  // namespace media
}  // namespace chromecast
