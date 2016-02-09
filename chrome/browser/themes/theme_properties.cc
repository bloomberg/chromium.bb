// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_properties.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "grit/theme_resources.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

// ----------------------------------------------------------------------------
// Defaults for properties which are stored in the browser theme pack. If you
// change these defaults, you must increment the version number in
// browser_theme_pack.h

// Default colors.
#if defined(OS_CHROMEOS)
// Used for theme fallback colors.
const SkColor kDefaultColorFrame[] = {
    SkColorSetRGB(0xC3, 0xC3, 0xC4), SkColorSetRGB(204, 204, 204)};
const SkColor kDefaultColorFrameInactive[] = {
    SkColorSetRGB(0xCD, 0xCD, 0xCE), SkColorSetRGB(220, 220, 220)};
#elif defined(OS_MACOSX)
const SkColor kDefaultColorFrame = SkColorSetRGB(224, 224, 224);
const SkColor kDefaultColorFrameInactive = SkColorSetRGB(246, 246, 246);
#else
const SkColor kDefaultColorFrame = SkColorSetRGB(66, 116, 201);
const SkColor kDefaultColorFrameInactive = SkColorSetRGB(161, 182, 228);
#endif  // OS_CHROMEOS

// These colors are the same between CrOS and !CrOS for MD, so this ifdef can be
// removed when we stop supporting pre-MD.
#if defined(OS_CHROMEOS)
const SkColor kDefaultColorFrameIncognito[] = {SkColorSetRGB(0xA0, 0xA0, 0xA4),
                                               SkColorSetRGB(0x28, 0x2B, 0x2D)};
const SkColor kDefaultColorFrameIncognitoInactive[] = {
    SkColorSetRGB(0xAA, 0xAA, 0xAE), SkColorSetRGB(0x14, 0x17, 0x19)};
#else
const SkColor kDefaultColorFrameIncognito[] = {SkColorSetRGB(83, 106, 139),
                                               SkColorSetRGB(0x28, 0x2B, 0x2D)};
const SkColor kDefaultColorFrameIncognitoInactive[] = {
    SkColorSetRGB(126, 139, 156), SkColorSetRGB(0x14, 0x17, 0x19)};
#endif

#if defined(OS_MACOSX)
const SkColor kDefaultColorToolbar = SkColorSetRGB(230, 230, 230);
#else
const SkColor kDefaultColorToolbar[] = {
    SkColorSetRGB(223, 223, 223), SkColorSetRGB(242, 242, 242)};
const SkColor kDefaultColorToolbarIncognito[] = {
    SkColorSetRGB(223, 223, 223), SkColorSetRGB(0x50, 0x50, 0x50)};
#endif  // OS_MACOSX
const SkColor kDefaultDetachedBookmarkBarBackground[] = {
    SkColorSetRGB(0xF1, 0xF1, 0xF1), SK_ColorWHITE};
const SkColor kDefaultDetachedBookmarkBarBackgroundIncognito[] = {
    SkColorSetRGB(0xF1, 0xF1, 0xF1), SkColorSetRGB(0x32, 0x32, 0x32)};

const SkColor kDefaultColorTabText = SK_ColorBLACK;
const SkColor kDefaultColorTabTextIncognito[] = {SK_ColorBLACK, SK_ColorWHITE};

#if defined(OS_MACOSX)
const SkColor kDefaultColorBackgroundTabText = SK_ColorBLACK;
#else
const SkColor kDefaultColorBackgroundTabText[] = {
    SkColorSetRGB(64, 64, 64), SK_ColorBLACK };
const SkColor kDefaultColorBackgroundTabTextIncognito[] = {
    SkColorSetRGB(64, 64, 64), SK_ColorWHITE };
#endif  // OS_MACOSX

const SkColor kDefaultColorBookmarkText = SK_ColorBLACK;
const SkColor kDefaultColorBookmarkTextIncognito[] = {SK_ColorBLACK,
                                                      SK_ColorWHITE};

#if defined(OS_WIN)
const SkColor kDefaultColorNTPBackground =
    color_utils::GetSysSkColor(COLOR_WINDOW);
const SkColor kDefaultColorNTPText =
    color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
const SkColor kDefaultColorNTPLink =
    color_utils::GetSysSkColor(COLOR_HOTLIGHT);
#else
// TODO(beng): source from theme provider.
const SkColor kDefaultColorNTPBackground = SK_ColorWHITE;
const SkColor kDefaultColorNTPText = SK_ColorBLACK;
const SkColor kDefaultColorNTPLink = SkColorSetRGB(6, 55, 116);
#endif  // OS_WIN

const SkColor kDefaultColorNTPHeader = SkColorSetRGB(150, 150, 150);
const SkColor kDefaultColorNTPSection = SkColorSetRGB(229, 229, 229);
const SkColor kDefaultColorNTPSectionText = SK_ColorBLACK;
const SkColor kDefaultColorNTPSectionLink = SkColorSetRGB(6, 55, 116);
const SkColor kDefaultColorButtonBackground = SkColorSetARGB(0, 0, 0, 0);

// Default tints.
const color_utils::HSL kDefaultTintButtons = { -1, -1, -1 };
const color_utils::HSL kDefaultTintButtonsIncognito = { -1, -1, 0.85 };
const color_utils::HSL kDefaultTintFrame = { -1, -1, -1 };
const color_utils::HSL kDefaultTintFrameInactive = { -1, -1, 0.75 };
const color_utils::HSL kDefaultTintFrameIncognito = { -1, 0.2, 0.35 };
const color_utils::HSL kDefaultTintFrameIncognitoInactive = { -1, 0.3, 0.6 };
const color_utils::HSL kDefaultTintBackgroundTab = { -1, -1, 0.4296875 };
const color_utils::HSL kDefaultTintBackgroundTabIncognito = { -1, -1, 0.34375 };

// ----------------------------------------------------------------------------
// Defaults for properties which are not stored in the browser theme pack.

const SkColor kDefaultColorControlBackground = SK_ColorWHITE;
const SkColor kDefaultDetachedBookmarkBarSeparator[] = {
    SkColorSetRGB(170, 170, 171), SkColorSetRGB(182, 180, 182)};
const SkColor kDefaultDetachedBookmarkBarSeparatorIncognito[] = {
    SkColorSetRGB(170, 170, 171), SkColorSetRGB(0x28, 0x28, 0x28)};
const SkColor kDefaultToolbarTopSeparator = SkColorSetA(SK_ColorBLACK, 0x40);

#if defined(OS_MACOSX)
const SkColor kDefaultColorToolbarButtonStroke = SkColorSetARGB(75, 81, 81, 81);
const SkColor kDefaultColorToolbarButtonStrokeInactive =
    SkColorSetARGB(75, 99, 99, 99);
const SkColor kDefaultColorToolbarBezel = SkColorSetRGB(204, 204, 204);
const SkColor kDefaultColorToolbarStroke = SkColorSetRGB(103, 103, 103);
const SkColor kDefaultColorToolbarStrokeInactive = SkColorSetRGB(163, 163, 163);
#endif  // OS_MACOSX

// ----------------------------------------------------------------------------

// Strings used in alignment properties.
const char kAlignmentCenter[] = "center";
const char kAlignmentTop[] = "top";
const char kAlignmentBottom[] = "bottom";
const char kAlignmentLeft[] = "left";
const char kAlignmentRight[] = "right";

// Strings used in background tiling repetition properties.
const char kTilingNoRepeat[] = "no-repeat";
const char kTilingRepeatX[] = "repeat-x";
const char kTilingRepeatY[] = "repeat-y";
const char kTilingRepeat[] = "repeat";

// The image resources that will be tinted by the 'button' tint value.
// If you change this list, you must increment the version number in
// browser_theme_pack.cc, and you should assign persistent IDs to the
// data table at the start of said file or else tinted versions of
// these resources will not be created.
//
// TODO(erg): The cocoa port is the last user of the IDR_*_[HP] variants. These
// should be removed once the cocoa port no longer uses them.
const int kToolbarButtonIDs[] = {
  IDR_BACK, IDR_BACK_D, IDR_BACK_H, IDR_BACK_P,
  IDR_FORWARD, IDR_FORWARD_D, IDR_FORWARD_H, IDR_FORWARD_P,
  IDR_HOME, IDR_HOME_H, IDR_HOME_P,
  IDR_RELOAD, IDR_RELOAD_H, IDR_RELOAD_P,
  IDR_STOP, IDR_STOP_D, IDR_STOP_H, IDR_STOP_P,
  IDR_BROWSER_ACTIONS_OVERFLOW, IDR_BROWSER_ACTIONS_OVERFLOW_H,
  IDR_BROWSER_ACTIONS_OVERFLOW_P,
  IDR_TOOLS, IDR_TOOLS_H, IDR_TOOLS_P,
  IDR_MENU_DROPARROW,
  IDR_TOOLBAR_BEZEL_HOVER, IDR_TOOLBAR_BEZEL_PRESSED, IDR_TOOLS_BAR,
};

SkColor TintForUnderline(SkColor input) {
  return SkColorSetA(input, SkColorGetA(input) / 3);
}

}  // namespace

// static
int ThemeProperties::StringToAlignment(const std::string& alignment) {
  int alignment_mask = 0;
  for (const std::string& component : base::SplitString(
           alignment, base::kWhitespaceASCII,
           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (base::LowerCaseEqualsASCII(component, kAlignmentTop))
      alignment_mask |= ALIGN_TOP;
    else if (base::LowerCaseEqualsASCII(component, kAlignmentBottom))
      alignment_mask |= ALIGN_BOTTOM;
    else if (base::LowerCaseEqualsASCII(component, kAlignmentLeft))
      alignment_mask |= ALIGN_LEFT;
    else if (base::LowerCaseEqualsASCII(component, kAlignmentRight))
      alignment_mask |= ALIGN_RIGHT;
  }
  return alignment_mask;
}

// static
int ThemeProperties::StringToTiling(const std::string& tiling) {
  if (base::LowerCaseEqualsASCII(tiling, kTilingRepeatX))
    return REPEAT_X;
  if (base::LowerCaseEqualsASCII(tiling, kTilingRepeatY))
    return REPEAT_Y;
  if (base::LowerCaseEqualsASCII(tiling, kTilingRepeat))
    return REPEAT;
  // NO_REPEAT is the default choice.
  return NO_REPEAT;
}

// static
std::string ThemeProperties::AlignmentToString(int alignment) {
  // Convert from an AlignmentProperty back into a string.
  std::string vertical_string(kAlignmentCenter);
  std::string horizontal_string(kAlignmentCenter);

  if (alignment & ALIGN_TOP)
    vertical_string = kAlignmentTop;
  else if (alignment & ALIGN_BOTTOM)
    vertical_string = kAlignmentBottom;

  if (alignment & ALIGN_LEFT)
    horizontal_string = kAlignmentLeft;
  else if (alignment & ALIGN_RIGHT)
    horizontal_string = kAlignmentRight;

  return horizontal_string + " " + vertical_string;
}

// static
std::string ThemeProperties::TilingToString(int tiling) {
  // Convert from a TilingProperty back into a string.
  if (tiling == REPEAT_X)
    return kTilingRepeatX;
  if (tiling == REPEAT_Y)
    return kTilingRepeatY;
  if (tiling == REPEAT)
    return kTilingRepeat;
  return kTilingNoRepeat;
}

// static
const std::set<int>& ThemeProperties::GetTintableToolbarButtons() {
  CR_DEFINE_STATIC_LOCAL(std::set<int>, button_set, ());
  if (button_set.empty()) {
    button_set = std::set<int>(
        kToolbarButtonIDs,
        kToolbarButtonIDs + arraysize(kToolbarButtonIDs));
  }

  return button_set;
}

// static
color_utils::HSL ThemeProperties::GetDefaultTint(int id, bool otr) {
  bool otr_tint = otr && ui::MaterialDesignController::IsModeMaterial();
  switch (id) {
    case TINT_FRAME:
      return otr_tint ? kDefaultTintFrameIncognito : kDefaultTintFrame;
    case TINT_FRAME_INACTIVE:
      return otr_tint ? kDefaultTintFrameIncognitoInactive
                      : kDefaultTintFrameInactive;
    case TINT_BUTTONS:
      return otr_tint ? kDefaultTintButtonsIncognito : kDefaultTintButtons;
    case TINT_BACKGROUND_TAB:
      return otr_tint ? kDefaultTintBackgroundTabIncognito
                      : kDefaultTintBackgroundTab;
    case TINT_FRAME_INCOGNITO:
    case TINT_FRAME_INCOGNITO_INACTIVE:
      NOTREACHED() << "These values should be queried via their respective "
                      "non-incognito equivalents and an appropriate |otr| "
                      "value.";
    default:
      return {-1, -1, -1};
  }
}

// static
SkColor ThemeProperties::GetDefaultColor(int id, bool otr) {
  int mode = ui::MaterialDesignController::IsModeMaterial();
  switch (id) {
    // Properties stored in theme pack.
    case COLOR_FRAME:
      if (otr)
        return kDefaultColorFrameIncognito[mode];
#if defined(OS_CHROMEOS)
      return kDefaultColorFrame[mode];
#else
      return kDefaultColorFrame;
#endif  // OS_CHROMEOS
    case COLOR_FRAME_INACTIVE:
      if (otr)
        return kDefaultColorFrameIncognitoInactive[mode];
#if defined(OS_CHROMEOS)
      return kDefaultColorFrameInactive[mode];
#else
      return kDefaultColorFrameInactive;
#endif  // OS_CHROMEOS
#if defined(OS_MACOSX)
    case COLOR_TOOLBAR:
      return kDefaultColorToolbar;
#else
    case COLOR_TOOLBAR:
      return otr ? kDefaultColorToolbarIncognito[mode]
                 : kDefaultColorToolbar[mode];
#endif  // OS_MACOSX
    case COLOR_TAB_TEXT:
      return otr ? kDefaultColorTabTextIncognito[mode]
                 : kDefaultColorTabText;
    case COLOR_BACKGROUND_TAB_TEXT:
#if defined(OS_MACOSX)
      return kDefaultColorBackgroundTabText;
#else
      return otr ? kDefaultColorBackgroundTabTextIncognito[mode]
                 : kDefaultColorBackgroundTabText[mode];
#endif  // OS_MACOSX
    case COLOR_BOOKMARK_TEXT:
      return otr ? kDefaultColorBookmarkTextIncognito[mode]
                 : kDefaultColorBookmarkText;
    case COLOR_NTP_BACKGROUND:
      return kDefaultColorNTPBackground;
    case COLOR_NTP_TEXT:
      return kDefaultColorNTPText;
    case COLOR_NTP_LINK:
      return kDefaultColorNTPLink;
    case COLOR_NTP_LINK_UNDERLINE:
      return TintForUnderline(kDefaultColorNTPLink);
    case COLOR_NTP_HEADER:
      return kDefaultColorNTPHeader;
    case COLOR_NTP_SECTION:
      return kDefaultColorNTPSection;
    case COLOR_NTP_SECTION_TEXT:
      return kDefaultColorNTPSectionText;
    case COLOR_NTP_SECTION_LINK:
      return kDefaultColorNTPSectionLink;
    case COLOR_NTP_SECTION_LINK_UNDERLINE:
      return TintForUnderline(kDefaultColorNTPSectionLink);
    case COLOR_BUTTON_BACKGROUND:
      return kDefaultColorButtonBackground;

    // Properties not stored in theme pack.
    case COLOR_CONTROL_BACKGROUND:
      return kDefaultColorControlBackground;
    case COLOR_TOOLBAR_BOTTOM_SEPARATOR:
    case COLOR_DETACHED_BOOKMARK_BAR_SEPARATOR:
      return otr ? kDefaultDetachedBookmarkBarSeparatorIncognito[mode]
                 : kDefaultDetachedBookmarkBarSeparator[mode];
    case COLOR_DETACHED_BOOKMARK_BAR_BACKGROUND:
      return otr ? kDefaultDetachedBookmarkBarBackgroundIncognito[mode]
                 : kDefaultDetachedBookmarkBarBackground[mode];
    case COLOR_TOOLBAR_TOP_SEPARATOR:
      return kDefaultToolbarTopSeparator;
#if defined(OS_MACOSX)
    case COLOR_TOOLBAR_BUTTON_STROKE:
      return kDefaultColorToolbarButtonStroke;
    case COLOR_TOOLBAR_BUTTON_STROKE_INACTIVE:
      return kDefaultColorToolbarButtonStrokeInactive;
    case COLOR_TOOLBAR_BEZEL:
      return kDefaultColorToolbarBezel;
    case COLOR_TOOLBAR_STROKE:
      return kDefaultColorToolbarStroke;
    case COLOR_TOOLBAR_STROKE_INACTIVE:
      return kDefaultColorToolbarStrokeInactive;
#endif
    case COLOR_FRAME_INCOGNITO:
    case COLOR_FRAME_INCOGNITO_INACTIVE:
      NOTREACHED() << "These values should be queried via their respective "
                      "non-incognito equivalents and an appropriate |otr| "
                      "value.";
    default:
      return gfx::kPlaceholderColor;
  }
}
