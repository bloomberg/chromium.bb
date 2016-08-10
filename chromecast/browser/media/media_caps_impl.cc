// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/media_caps_impl.h"

#include "chromecast/media/base/media_caps.h"

namespace chromecast {
namespace media {

MediaCapsImpl::MediaCapsImpl() : supported_codecs_bitmask_(0) {}

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

  observers_.ForAllPtrs(
      [width, height](mojom::MediaCapsObserver* observer) {
        observer->ScreenResolutionChanged(width, height);
      });
}

void MediaCapsImpl::AddObserver(mojom::MediaCapsObserverPtr observer) {
  observer->SupportedHdmiSinkCodecsChanged(supported_codecs_bitmask_);
  observer->ScreenResolutionChanged(screen_resolution_.width(),
                                    screen_resolution_.height());
  observers_.AddPtr(std::move(observer));
}

}  // namespace media
}  // namespace chromecast
