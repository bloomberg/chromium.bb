// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_properties.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

// ----------------------------------------------------------------------------
// Defaults for properties which are stored in the browser theme pack. If you
// change these defaults, you must increment the version number in
// browser_theme_pack.h

// Default colors.
#if defined(OS_CHROMEOS)
// Used for theme fallback colors.
const SkColor kDefaultColorFrame = SkColorSetRGB(109, 109, 109);
const SkColor kDefaultColorFrameInactive = SkColorSetRGB(176, 176, 176);
#elif defined(OS_MACOSX)
const SkColor kDefaultColorFrame = SkColorSetRGB(224, 224, 224);
const SkColor kDefaultColorFrameInactive = SkColorSetRGB(246, 246, 246);
#else
const SkColor kDefaultColorFrame = SkColorSetRGB(66, 116, 201);
const SkColor kDefaultColorFrameInactive = SkColorSetRGB(161, 182, 228);
#endif  // OS_CHROMEOS
const SkColor kDefaultColorFrameIncognito = SkColorSetRGB(83, 106, 139);
const SkColor kDefaultColorFrameIncognitoInactive =
    SkColorSetRGB(126, 139, 156);
#if defined(OS_MACOSX)
const SkColor kDefaultColorToolbar = SkColorSetRGB(230, 230, 230);
#else
const SkColor kDefaultColorToolbar = SkColorSetRGB(223, 223, 223);
#endif
const SkColor kDefaultColorTabText = SK_ColorBLACK;
#if defined(OS_MACOSX)
const SkColor kDefaultColorBackgroundTabText = SK_ColorBLACK;
#else
const SkColor kDefaultColorBackgroundTabText = SkColorSetRGB(64, 64, 64);
#endif
const SkColor kDefaultColorBookmarkText = SK_ColorBLACK;
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
#endif
const SkColor kDefaultColorNTPHeader = SkColorSetRGB(150, 150, 150);
const SkColor kDefaultColorNTPSection = SkColorSetRGB(229, 229, 229);
const SkColor kDefaultColorNTPSectionText = SK_ColorBLACK;
const SkColor kDefaultColorNTPSectionLink = SkColorSetRGB(6, 55, 116);
const SkColor kDefaultColorButtonBackground = SkColorSetARGB(0, 0, 0, 0);

// Default tints.
const color_utils::HSL kDefaultTintButtons = { -1, -1, -1 };
const color_utils::HSL kDefaultTintFrame = { -1, -1, -1 };
const color_utils::HSL kDefaultTintFrameInactive = { -1, -1, 0.75f };
const color_utils::HSL kDefaultTintFrameIncognito = { -1, 0.2f, 0.35f };
const color_utils::HSL kDefaultTintFrameIncognitoInactive = { -1, 0.3f, 0.6f };
const color_utils::HSL kDefaultTintBackgroundTab = { -1, 0.5, 0.75 };

// Default display properties.
const int kDefaultDisplayPropertyNTPAlignment =
    ThemeProperties::ALIGN_CENTER;
const int kDefaultDisplayPropertyNTPTiling =
    ThemeProperties::NO_REPEAT;
// By default, we do not use the ntp alternate logo.
const int kDefaultDisplayPropertyNTPAlternateLogo = 0;

// ----------------------------------------------------------------------------
// Defaults for properties which are not stored in the browser theme pack.

const SkColor kDefaultColorControlBackground = SK_ColorWHITE;
const SkColor kDefaultColorToolbarSeparator = SkColorSetRGB(170, 170, 171);

#if defined(OS_MACOSX)
const SkColor kDefaultColorToolbarButtonStroke = SkColorSetARGB(75, 81, 81, 81);
const SkColor kDefaultColorToolbarButtonStrokeInactive =
    SkColorSetARGB(75, 99, 99, 99);
const SkColor kDefaultColorToolbarBezel = SkColorSetRGB(204, 204, 204);
const SkColor kDefaultColorToolbarStroke = SkColorSetRGB(103, 103, 103);
const SkColor kDefaultColorToolbarStrokeInactive = SkColorSetRGB(163, 163, 163);
#endif

// ----------------------------------------------------------------------------
// Defaults for layout properties which are not stored in the browser theme
// pack. The array indices here are the values of
// ui::MaterialDesignController::Mode, see
// ui/base/resource/material_design/material_design_controller.h

// Additional horizontal padding applied on the trailing edge of icon-label
// views.
const int kIconLabelViewTrailingPadding[] = {2, 8, 8};

// The horizontal space between the edge and a bubble
const int kLocationBarBubbleHorizontalPadding[] = {1, 5, 5};

// The additional vertical padding of a bubble.
const int kLocationBarBubbleVerticalPadding[] = {1, 3, 3};

// The height to be occupied by the LocationBar. For
// MaterialDesignController::NON_MATERIAL the height is determined from image
// assets.
const int kLocationBarHeight[] = {0, 28, 32};

// Space between items in the location bar, as well as between items and the
// edges.
const int kLocationBarHorizontalPadding[] = {3, 6, 6};

// The Vertical padding of items in the location bar.
const int kLocationBarVerticalPadding[] = {2, 6, 6};

// The number of pixels in the omnibox dropdown border image interior to
// the actual border.
const int kOmniboxDropdownBorderInterior[] = {6, 6, 6};

// In an omnibox dropdown row, the minimum distance between the top and
// bottom of the row's icon and the top or bottom of the row edge.
const int kOmniboxDropdownMinIconVerticalPadding[] = {2, 4, 8};

// In an omnibox dropdown row, the minimum distance between the top and
// bottom of the row's text and the top or bottom of the row edge.
const int kOmniboxDropdownMinTextVerticalPadding[] = {3, 4, 8};

// The spacing between a ToolbarButton's image and its border.
const int kToolbarButtonBorderInset[] = {2, 6, 6};

// Non-ash uses a rounded content area with no shadow in the assets.
const int kToolbarViewContentShadowHeight[] = {0, 0, 0};

// Ash doesn't use a rounded content area and its top edge has an extra shadow.
const int kToolbarViewContentShadowHeightAsh[] = {2, 0, 0};

// Additional horizontal padding between the elements in the toolbar.
const int kToolbarViewElementPadding[] = {0, 0, 8};

// Padding between the right-edge of the location bar and browser actions.
const int kToolbarViewLocationBarRightPadding[] = {0, 4, 8};

// The edge graphics have some built-in spacing/shadowing, so we have to adjust
// our spacing to make it match.
const int kToolbarViewLeftEdgeSpacing[] = {3, 4, 8};
const int kToolbarViewRightEdgeSpacing[] = {2, 4, 8};

// The horizontal space between most items.
const int kToolbarViewStandardSpacing[] = {3, 4, 8};

// The minimal vertical padding of the toolbar.
const int kToolbarViewVerticalPadding[] = {5, 4, 4};

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
color_utils::HSL ThemeProperties::GetDefaultTint(int id) {
  switch (id) {
    case TINT_FRAME:
      return kDefaultTintFrame;
    case TINT_FRAME_INACTIVE:
      return kDefaultTintFrameInactive;
    case TINT_FRAME_INCOGNITO:
      return kDefaultTintFrameIncognito;
    case TINT_FRAME_INCOGNITO_INACTIVE:
      return kDefaultTintFrameIncognitoInactive;
    case TINT_BUTTONS:
      return kDefaultTintButtons;
    case TINT_BACKGROUND_TAB:
      return kDefaultTintBackgroundTab;
    default:
      color_utils::HSL result = {-1, -1, -1};
      return result;
  }
}

// static
SkColor ThemeProperties::GetDefaultColor(int id) {
  switch (id) {
    // Properties stored in theme pack.
    case COLOR_FRAME:
      return kDefaultColorFrame;
    case COLOR_FRAME_INACTIVE:
      return kDefaultColorFrameInactive;
    case COLOR_FRAME_INCOGNITO:
      return kDefaultColorFrameIncognito;
    case COLOR_FRAME_INCOGNITO_INACTIVE:
      return kDefaultColorFrameIncognitoInactive;
    case COLOR_TOOLBAR:
      return kDefaultColorToolbar;
    case COLOR_TAB_TEXT:
      return kDefaultColorTabText;
    case COLOR_BACKGROUND_TAB_TEXT:
      return kDefaultColorBackgroundTabText;
    case COLOR_BOOKMARK_TEXT:
      return kDefaultColorBookmarkText;
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
    case COLOR_TOOLBAR_SEPARATOR:
      return kDefaultColorToolbarSeparator;
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
    default:
      // Return a debugging red color.
      return SK_ColorRED;
  }
}

// static
int ThemeProperties::GetDefaultDisplayProperty(int id) {
  int mode = ui::MaterialDesignController::GetMode();
  switch (id) {
    case ThemeProperties::NTP_BACKGROUND_ALIGNMENT:
      return kDefaultDisplayPropertyNTPAlignment;
    case ThemeProperties::NTP_BACKGROUND_TILING:
      return kDefaultDisplayPropertyNTPTiling;
    case ThemeProperties::NTP_LOGO_ALTERNATE:
      return kDefaultDisplayPropertyNTPAlternateLogo;
    case ThemeProperties::PROPERTY_ICON_LABEL_VIEW_TRAILING_PADDING:
      return kIconLabelViewTrailingPadding[mode];
    case ThemeProperties::PROPERTY_LOCATION_BAR_BUBBLE_HORIZONTAL_PADDING:
      return kLocationBarBubbleHorizontalPadding[mode];
    case ThemeProperties::PROPERTY_LOCATION_BAR_BUBBLE_VERTICAL_PADDING:
      return kLocationBarBubbleVerticalPadding[mode];
    case ThemeProperties::PROPERTY_LOCATION_BAR_HEIGHT:
      return kLocationBarHeight[mode];
    case ThemeProperties::PROPERTY_LOCATION_BAR_HORIZONTAL_PADDING:
      return kLocationBarHorizontalPadding[mode];
    case ThemeProperties::PROPERTY_LOCATION_BAR_VERTICAL_PADDING:
      return kLocationBarVerticalPadding[mode];
    case ThemeProperties::PROPERTY_OMNIBOX_DROPDOWN_BORDER_INTERIOR:
      return kOmniboxDropdownBorderInterior[mode];
    case ThemeProperties::PROPERTY_OMNIBOX_DROPDOWN_MIN_ICON_VERTICAL_PADDING:
      return kOmniboxDropdownMinIconVerticalPadding[mode];
    case ThemeProperties::PROPERTY_OMNIBOX_DROPDOWN_MIN_TEXT_VERTICAL_PADDING:
      return kOmniboxDropdownMinTextVerticalPadding[mode];
    case ThemeProperties::PROPERTY_TOOLBAR_BUTTON_BORDER_INSET:
      return kToolbarButtonBorderInset[mode];
    case ThemeProperties::PROPERTY_TOOLBAR_VIEW_CONTENT_SHADOW_HEIGHT:
      return kToolbarViewContentShadowHeight[mode];
    case ThemeProperties::PROPERTY_TOOLBAR_VIEW_CONTENT_SHADOW_HEIGHT_ASH:
      return kToolbarViewContentShadowHeightAsh[mode];
    case ThemeProperties::PROPERTY_TOOLBAR_VIEW_ELEMENT_PADDING:
      return kToolbarViewElementPadding[mode];
    case ThemeProperties::PROPERTY_TOOLBAR_VIEW_LEFT_EDGE_SPACING:
      return kToolbarViewLeftEdgeSpacing[mode];
    case ThemeProperties::PROPERTY_TOOLBAR_VIEW_LOCATION_BAR_RIGHT_PADDING:
      return kToolbarViewLocationBarRightPadding[mode];
    case ThemeProperties::PROPERTY_TOOLBAR_VIEW_RIGHT_EDGE_SPACING:
      return kToolbarViewRightEdgeSpacing[mode];
    case ThemeProperties::PROPERTY_TOOLBAR_VIEW_STANDARD_SPACING:
      return kToolbarViewStandardSpacing[mode];
    case ThemeProperties::PROPERTY_TOOLBAR_VIEW_VERTICAL_PADDING:
      return kToolbarViewVerticalPadding[mode];
    default:
      return -1;
  }
}
