// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_
#define CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"

// Static only class for querying which properties / images are themeable and
// the defaults of these properties.
// All methods are thread safe unless indicated otherwise.
class ThemeProperties {
 public:
  // ---------------------------------------------------------------------------
  // The int values of OverwritableByUserThemeProperties, Alignment, and Tiling
  // are used as a key to store the property in the browser theme pack. If you
  // modify any of these enums, increment the version number in
  // browser_theme_pack.cc.

  enum OverwritableByUserThemeProperty {
    COLOR_FRAME,
    COLOR_FRAME_INACTIVE,
    // Instead of using the INCOGNITO variants directly, most code should
    // use the original color ID in an incognito-aware context (such as
    // GetDefaultColor).
    COLOR_FRAME_INCOGNITO,
    COLOR_FRAME_INCOGNITO_INACTIVE,
    COLOR_TOOLBAR,
    COLOR_TAB_TEXT,
    COLOR_BACKGROUND_TAB_TEXT,
    COLOR_BOOKMARK_TEXT,
    COLOR_NTP_BACKGROUND,
    COLOR_NTP_TEXT,
    COLOR_NTP_LINK,
    COLOR_NTP_HEADER,
    COLOR_BUTTON_BACKGROUND,

    TINT_BUTTONS,
    TINT_FRAME,
    TINT_FRAME_INACTIVE,
    TINT_FRAME_INCOGNITO,
    TINT_FRAME_INCOGNITO_INACTIVE,
    TINT_BACKGROUND_TAB,

    NTP_BACKGROUND_ALIGNMENT,
    NTP_BACKGROUND_TILING,
    NTP_LOGO_ALTERNATE
  };

  // A bitfield mask for alignments.
  enum Alignment {
    ALIGN_CENTER = 0,
    ALIGN_LEFT   = 1 << 0,
    ALIGN_TOP    = 1 << 1,
    ALIGN_RIGHT  = 1 << 2,
    ALIGN_BOTTOM = 1 << 3,
  };

  // Background tiling choices.
  enum Tiling {
    NO_REPEAT = 0,
    REPEAT_X = 1,
    REPEAT_Y = 2,
    REPEAT = 3
  };

  // --------------------------------------------------------------------------
  // The int value of the properties in NotOverwritableByUserThemeProperties
  // has no special meaning. Modify the enum to your heart's content.
  // The enum takes on values >= 1000 as not to overlap with
  // OverwritableByUserThemeProperties.
  enum NotOverwritableByUserThemeProperty {
    COLOR_CONTROL_BACKGROUND = 1000,

    // The color of the border drawn around the location bar.
    COLOR_LOCATION_BAR_BORDER,

    // The color of the line separating the bottom of the toolbar from the
    // contents.
    COLOR_TOOLBAR_BOTTOM_SEPARATOR,

    // The color of a normal toolbar button's icon.
    COLOR_TOOLBAR_BUTTON_ICON,
    // The color of a disabled toolbar button's icon.
    COLOR_TOOLBAR_BUTTON_ICON_INACTIVE,

    // The color of the line separating the top of the toolbar from the region
    // above. For a tabbed browser window, this is the line along the bottom
    // edge of the tabstrip, the stroke around the tabs, and the new tab button
    // stroke/shadow color.
    COLOR_TOOLBAR_TOP_SEPARATOR,
    COLOR_TOOLBAR_TOP_SEPARATOR_INACTIVE,

    // Colors of vertical separators, such as on the bookmark bar or on the DL
    // shelf.
    COLOR_TOOLBAR_VERTICAL_SEPARATOR,

    // The color of a background tab, as well as the new tab button.
    COLOR_BACKGROUND_TAB,

    // The color of the "instructions text" in an empty bookmarks bar.
    COLOR_BOOKMARK_BAR_INSTRUCTIONS_TEXT,

    // Colors used for the detached (NTP) bookmark bar.
    COLOR_DETACHED_BOOKMARK_BAR_BACKGROUND,
    COLOR_DETACHED_BOOKMARK_BAR_SEPARATOR,

    // The throbber colors for tabs or anything on a toolbar (currently, only
    // the download shelf). If you're adding a throbber elsewhere, such as in
    // a dialog or bubble, you likely want
    // NativeTheme::kColorId_ThrobberSpinningColor.
    COLOR_TAB_THROBBER_SPINNING,
    COLOR_TAB_THROBBER_WAITING,

    // Colors for the tab close button inons.
    COLOR_TAB_CLOSE_BUTTON_BACKGROUND_ACTIVE,
    COLOR_TAB_CLOSE_BUTTON_BACKGROUND_INACTIVE,
    COLOR_TAB_CLOSE_BUTTON_BACKGROUND_HOVER,
    COLOR_TAB_CLOSE_BUTTON_BACKGROUND_PRESSED,

    // The colors used by the various alert indicator icons in the tab.
    COLOR_TAB_ALERT_AUDIO,
    COLOR_TAB_ALERT_RECORDING,
    COLOR_TAB_ALERT_CAPTURING,

    // These colors don't have constant default values. They are derived from
    // the runtime value of other colors.
    COLOR_NTP_TEXT_LIGHT,
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    COLOR_SUPERVISED_USER_LABEL,
    COLOR_SUPERVISED_USER_LABEL_BACKGROUND,
    COLOR_SUPERVISED_USER_LABEL_BORDER,
#endif

#if defined(OS_MACOSX)
    COLOR_FRAME_VIBRANCY_OVERLAY,
    COLOR_TOOLBAR_INACTIVE,
    COLOR_BACKGROUND_TAB_INACTIVE,
    COLOR_TOOLBAR_BEZEL,
    COLOR_TOOLBAR_STROKE,
    COLOR_TOOLBAR_STROKE_INACTIVE,
    COLOR_TOOLBAR_STROKE_THEME,
    COLOR_TOOLBAR_STROKE_THEME_INACTIVE,
    // The color of a toolbar button's border.
    COLOR_TOOLBAR_BUTTON_STROKE,
    COLOR_TOOLBAR_BUTTON_STROKE_INACTIVE,
    GRADIENT_TOOLBAR,
    GRADIENT_TOOLBAR_INACTIVE,
    GRADIENT_TOOLBAR_BUTTON,
    GRADIENT_TOOLBAR_BUTTON_INACTIVE,
    GRADIENT_TOOLBAR_BUTTON_PRESSED,
    GRADIENT_TOOLBAR_BUTTON_PRESSED_INACTIVE,
#endif  // OS_MACOSX

#if defined(OS_WIN)
    // The color of the 1px border around the window on Windows 10.
    COLOR_ACCENT_BORDER,
#endif  // OS_WIN
  };

  // Used by the browser theme pack to parse alignments from something like
  // "top left" into a bitmask of Alignment.
  static int StringToAlignment(const std::string& alignment);

  // Used by the browser theme pack to parse alignments from something like
  // "no-repeat" into a Tiling value.
  static int StringToTiling(const std::string& tiling);

  // Converts a bitmask of Alignment into a string like "top left". The result
  // is used to generate a CSS value.
  static std::string AlignmentToString(int alignment);

  // Converts a Tiling into a string like "no-repeat". The result is used to
  // generate a CSS value.
  static std::string TilingToString(int tiling);

  // Returns the default tint for the given tint |id| TINT_* enum value.
  // Returns an HSL value of {-1, -1, -1} if |id| is invalid.
  static color_utils::HSL GetDefaultTint(int id, bool incognito);

  // Returns the default color for the given color |id| COLOR_* enum value.
  // Returns gfx::kPlaceholderColor if |id| is invalid.
  static SkColor GetDefaultColor(int id, bool incognito);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ThemeProperties);
};

#endif  // CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_
