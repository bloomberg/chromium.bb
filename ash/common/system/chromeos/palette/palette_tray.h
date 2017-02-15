// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_PALETTE_BUTTON_TRAY_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_PALETTE_BUTTON_TRAY_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/common/palette_delegate.h"
#include "ash/common/session/session_state_observer.h"
#include "ash/common/shell_observer.h"
#include "ash/common/system/chromeos/palette/palette_tool_manager.h"
#include "ash/common/system/tray/tray_background_view.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace gfx {
class Point;
}

namespace views {
class ImageView;
class Widget;
}

namespace ash {

class TrayBubbleWrapper;
class PaletteToolManager;

// The PaletteTray shows the palette in the bottom area of the screen. This
// class also controls the lifetime for all of the tools available in the
// palette.
class ASH_EXPORT PaletteTray : public TrayBackgroundView,
                               public SessionStateObserver,
                               public ShellObserver,
                               public PaletteToolManager::Delegate,
                               public ui::InputDeviceEventObserver,
                               public views::TrayBubbleView::Delegate {
 public:
  explicit PaletteTray(WmShelf* wm_shelf);
  ~PaletteTray() override;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  // SessionStateObserver:
  void SessionStateChanged(session_manager::SessionState state) override;

  // ShellObserver:
  void OnLockStateChanged(bool locked) override;

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view) override;
  void SetShelfAlignment(ShelfAlignment alignment) override;
  void AnchorUpdated() override;
  void Initialize() override;

  // PaletteToolManager::Delegate:
  void HidePalette() override;
  void HidePaletteImmediately() override;
  void RecordPaletteOptionsUsage(PaletteTrayOptions option) override;
  void RecordPaletteModeCancellation(PaletteModeCancelType type) override;

  // Returns true if the shelf should not autohide.
  bool ShouldBlockShelfAutoHide() const;

  // Opens up the palette if it is not already open. Returns true if the palette
  // was opened.
  bool ShowPalette();

  // Returns true if the palette tray contains the given point. This is useful
  // for determining if an event should be propagated through to the palette.
  bool ContainsPointInScreen(const gfx::Point& point);

 private:
  // ui::InputDeviceObserver:
  void OnTouchscreenDeviceConfigurationChanged() override;
  void OnStylusStateChanged(ui::StylusState stylus_state) override;

  // views::TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  void OnMouseEnteredView() override;
  void OnMouseExitedView() override;
  base::string16 GetAccessibleNameForBubble() override;
  void OnBeforeBubbleWidgetInit(
      views::Widget* anchor_widget,
      views::Widget* bubble_widget,
      views::Widget::InitParams* params) const override;
  void HideBubble(const views::TrayBubbleView* bubble_view) override;

  // PaletteToolManager::Delegate:
  void OnActiveToolChanged() override;
  WmWindow* GetWindow() override;

  // Updates the tray icon from the palette tool manager.
  void UpdateTrayIcon();

  // Sets the icon to visible if the palette can be used.
  void UpdateIconVisibility();

  // Called when the palette enabled pref has changed.
  void OnPaletteEnabledPrefChanged(bool enabled);

  std::unique_ptr<PaletteToolManager> palette_tool_manager_;
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // Manages the callback OnPaletteEnabledPrefChanged callback registered to
  // the PaletteDelegate instance.
  std::unique_ptr<PaletteDelegate::EnableListenerSubscription>
      palette_enabled_subscription_;

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  // The shelf auto-hide state is checked during the tray constructor, so we
  // have to use a helper variable instead of just checking if we have a tray
  // instance.
  bool should_block_shelf_auto_hide_ = false;

  // Cached palette enabled/disabled pref value.
  bool is_palette_enabled_ = true;

  // Used to indicate whether the palette bubble is automatically opened by a
  // stylus eject event.
  bool is_bubble_auto_opened_ = false;

  // Number of actions in pen palette bubble.
  int num_actions_in_bubble_ = 0;

  base::WeakPtrFactory<PaletteTray> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PaletteTray);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_PALETTE_BUTTON_TRAY_H_
