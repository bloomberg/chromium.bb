// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_ITEM_STYLE_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_ITEM_STYLE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {
class NativeTheme;
}  // namespace ui

namespace views {
class Label;
}  // namespace views

namespace ash {
class TrayPopupItemStyleObserver;

// Central style provider for the system tray menu. Makes it easier to ensure
// all visuals are consistent and easily updated in one spot instead of being
// defined in multiple places throughout the code.
class TrayPopupItemStyle {
 public:
  // The different visual styles that a row can have.
  enum class ColorStyle {
    // Active and clickable.
    ACTIVE,
    // Inactive but clickable.
    INACTIVE,
    // Disabled and not clickable.
    DISABLED,
  };

  // The different font styles that row text can have.
  enum class FontStyle {
    // Header rows for default view and detailed view.
    TITLE,
    // Main text used by default view rows.
    DEFAULT_VIEW_LABEL,
    // Main text used by detailed view rows.
    DETAILED_VIEW_LABEL,
    // System information text (e.g. date/time, battery status, etc).
    SYSTEM_INFO,
    // Sub text within a row (e.g. user name in user row).
    CAPTION,
    // Child buttons within rows that have a visible border (e.g. Cast's
    // "Stop", etc).
    BUTTON,
  };

  TrayPopupItemStyle(const ui::NativeTheme* theme, FontStyle font_style);
  ~TrayPopupItemStyle();

  void AddObserver(TrayPopupItemStyleObserver* observer);
  void RemoveObserver(TrayPopupItemStyleObserver* observer);

  const ui::NativeTheme* theme() const { return theme_; }

  // Sets the |theme_| and notifies observers.
  void SetTheme(const ui::NativeTheme* theme);

  ColorStyle color_style() const { return color_style_; }

  // Sets the |color_style_| and notifies observers if |color_style_| changed.
  void SetColorStyle(ColorStyle color_style);

  FontStyle font_style() const { return font_style_; }

  // Sets the |font_style_| notifies observers if |font_style_| changed.
  void SetFontStyle(FontStyle font_style);

  SkColor GetForegroundColor() const;

  // Configures a Label as per the style (e.g. color, font).
  void SetupLabel(views::Label* label) const;

 private:
  void NotifyObserversStyleUpdated();

  // The theme that the styles are dervied from.
  const ui::NativeTheme* theme_;

  FontStyle font_style_;

  ColorStyle color_style_;

  base::ObserverList<TrayPopupItemStyleObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupItemStyle);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_POPUP_ITEM_STYLE_H_
