// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_cast_mode.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

namespace {

std::string TruncateHostToRegisteredDomain(const std::string& host) {
  const std::string truncated =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  // The truncation will be empty in some scenarios (e.g. host is
  // simply an IP address). Fail gracefully.
  if (truncated.empty()) {
    return host;
  }
  return truncated;
}

}  // namespace

std::string MediaCastModeToTitle(MediaCastMode mode, const std::string& host) {
  switch (mode) {
    case MediaCastMode::DEFAULT:
      return l10n_util::GetStringFUTF8(
          IDS_MEDIA_ROUTER_DEFAULT_CAST_MODE_TITLE,
          base::UTF8ToUTF16(TruncateHostToRegisteredDomain(host)));
    case MediaCastMode::TAB_MIRROR:
      return l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_TAB_MIRROR_CAST_MODE_TITLE);
    case MediaCastMode::DESKTOP_MIRROR:
      return l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_DESKTOP_MIRROR_CAST_MODE_TITLE);
    default:
      NOTREACHED();
      return "";
  }
}

std::string MediaCastModeToDescription(
    MediaCastMode mode, const std::string& host) {
  switch (mode) {
    case MediaCastMode::DEFAULT:
      return l10n_util::GetStringFUTF8(
          IDS_MEDIA_ROUTER_DEFAULT_CAST_MODE,
          base::UTF8ToUTF16(TruncateHostToRegisteredDomain(host)));
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
