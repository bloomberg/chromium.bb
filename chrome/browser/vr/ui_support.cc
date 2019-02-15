// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_support.h"

#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/vr/vector_icons/vector_icons.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"

namespace vr {

UScriptCode UScriptGetScript(UChar32 codepoint, UErrorCode* err) {
  return uscript_getScript(codepoint, err);
}

base::string16 FormatUrlForVr(const GURL& gurl, url::Parsed* new_parsed) {
  return url_formatter::FormatUrl(
      gurl,

      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains,
      net::UnescapeRule::NORMAL, new_parsed, nullptr, nullptr);
}

const gfx::VectorIcon& GetVrIcon(VrIconId icon) {
  switch (icon) {
    case kVrNoneIcon:
      return gfx::kNoneIcon;
    case kVrReloadIcon:
      return vector_icons::kReloadIcon;
    case kVrVideocamIcon:
      return vector_icons::kVideocamIcon;
    case kVrBackArrowIcon:
      return vector_icons::kBackArrowIcon;
    case kVrInfoOutlineIcon:
      return vector_icons::kInfoOutlineIcon;
    case kVrScreenShareIcon:
      return vector_icons::kScreenShareIcon;
    case kVrCloseRoundedIcon:
      return vector_icons::kCloseRoundedIcon;
    case kVrForwardArrowIcon:
      return vector_icons::kForwardArrowIcon;
    case kVrBluetoothConnectedIcon:
      return vector_icons::kBluetoothConnectedIcon;
    case kVrMicIcon:
      return vector_icons::kMicIcon;
    case kVrMyLocationIcon:
      return kMyLocationIcon;
    case kVrRepositionIcon:
      return kRepositionIcon;
    case kVrMoreVertIcon:
      return kMoreVertIcon;
    case kVrSadTabIcon:
      return kSadTabIcon;
    case kVrRemoveCircleOutlineIcon:
      return kRemoveCircleOutlineIcon;
    case kVrDaydreamControllerAppButtonIcon:
      return kDaydreamControllerAppButtonIcon;
    case kVrDaydreamControllerHomeButtonIcon:
      return kDaydreamControllerHomeButtonIcon;
#if !defined(OS_ANDROID)
    case kVrOpenInBrowserIcon:
      return kOpenInBrowserIcon;
#endif
    default:
      NOTREACHED();
      return gfx::kNoneIcon;
  }
}

}  // namespace vr
