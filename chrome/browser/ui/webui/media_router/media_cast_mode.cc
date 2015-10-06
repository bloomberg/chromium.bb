// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_cast_mode.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

std::string MediaCastModeToDescription(
    MediaCastMode mode, const std::string& host) {
  switch (mode) {
    case MediaCastMode::DEFAULT:
      return l10n_util::GetStringFUTF8(
          IDS_MEDIA_ROUTER_DEFAULT_CAST_MODE,
          base::UTF8ToUTF16(host));
    case MediaCastMode::TAB_MIRROR:
      return l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_TAB_MIRROR_CAST_MODE);
    case MediaCastMode::DESKTOP_MIRROR:
      return l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_DESKTOP_MIRROR_CAST_MODE);
    default:
      NOTREACHED();
      return "";
  }
}

bool IsValidCastModeNum(int cast_mode_num) {
  return cast_mode_num >= MediaCastMode::DEFAULT &&
         cast_mode_num < MediaCastMode::NUM_CAST_MODES;
}

MediaCastMode GetPreferredCastMode(const CastModeSet& cast_modes) {
  if (cast_modes.empty()) {
    LOG(ERROR) << "Called with empty cast_modes!";
    return MediaCastMode::DEFAULT;
  }
  return *cast_modes.begin();
}

}  // namespace media_router
