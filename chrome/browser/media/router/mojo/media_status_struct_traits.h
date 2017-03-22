// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_STATUS_STRUCT_TRAITS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_STATUS_STRUCT_TRAITS_H_

#include <string>

#include "chrome/browser/media/router/media_status.h"
#include "chrome/browser/media/router/mojo/media_status.mojom.h"
#include "mojo/common/common_custom_types_struct_traits.h"

namespace mojo {

// MediaStatus

template <>
struct StructTraits<media_router::mojom::MediaStatusDataView,
                    media_router::MediaStatus> {
  static bool Read(media_router::mojom::MediaStatusDataView data,
                   media_router::MediaStatus* out);

  static const std::string& title(const media_router::MediaStatus& status) {
    return status.title;
  }

  static const std::string& description(
      const media_router::MediaStatus& status) {
    return status.description;
  }

  static bool can_play_pause(const media_router::MediaStatus& status) {
    return status.can_play_pause;
  }

  static bool can_mute(const media_router::MediaStatus& status) {
    return status.can_mute;
  }

  static bool can_set_volume(const media_router::MediaStatus& status) {
    return status.can_set_volume;
  }

  static bool can_seek(const media_router::MediaStatus& status) {
    return status.can_seek;
  }

  static bool is_paused(const media_router::MediaStatus& status) {
    return status.is_paused;
  }

  static bool is_muted(const media_router::MediaStatus& status) {
    return status.is_muted;
  }

  static float volume(const media_router::MediaStatus& status) {
    return status.volume;
  }

  static base::TimeDelta duration(const media_router::MediaStatus& status) {
    return status.duration;
  }

  static base::TimeDelta current_time(const media_router::MediaStatus& status) {
    return status.current_time;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_STATUS_STRUCT_TRAITS_H_
