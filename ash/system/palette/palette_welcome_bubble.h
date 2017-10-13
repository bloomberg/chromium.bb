// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_PALETTE_WELCOME_BUBBLE_H_
#define ASH_SYSTEM_PALETTE_PALETTE_WELCOME_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/widget/widget_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace views {
class ImageButton;
class Widget;
}  // namespace views

namespace ash {
class PaletteTray;

// The PaletteWelcomeBubble handles displaying a warm welcome bubble letting
// users know about the PaletteTray the first time a stylus is ejected, or if an
// external stylus is detected.
class ASH_EXPORT PaletteWelcomeBubble : public SessionObserver,
                                        public views::PointerWatcher,
                                        public views::WidgetObserver {
 public:
  explicit PaletteWelcomeBubble(PaletteTray* tray);
  ~PaletteWelcomeBubble() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Show the welcome bubble iff it has not been shown before.
  void ShowIfNeeded();

  bool BubbleShown() { return bubble_view_ != nullptr; }

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  // views::PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              gfx::NativeView target) override;

  // Returns the close button on the bubble if it exists.
  views::ImageButton* GetCloseButtonForTest();

  // Returns the bounds of the bubble view if it exists.
  base::Optional<gfx::Rect> GetBubbleBoundsForTest();

 private:
  friend class PaletteWelcomeBubbleTest;
  class WelcomeBubbleView;

  void Show();
  void Hide();

  // The PaletteTray this bubble is associated with. Serves as the anchor for
  // the bubble. Not owned.
  PaletteTray* tray_ = nullptr;

  PrefService* active_user_pref_service_ = nullptr;  // Not owned.

  WelcomeBubbleView* bubble_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PaletteWelcomeBubble);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_PALETTE_WELCOME_BUBBLE_H_
