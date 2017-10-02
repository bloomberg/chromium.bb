// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_PALETTE_TRAY_H_
#define ASH_SYSTEM_PALETTE_PALETTE_TRAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/palette_delegate.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/palette/palette_tool_manager.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/devices/input_device_event_observer.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace gfx {
class Point;
}

namespace views {
class ImageView;
}

namespace ash {

class PaletteToolManager;
class TrayBubbleWrapper;

// The PaletteTray shows the palette in the bottom area of the screen. This
// class also controls the lifetime for all of the tools available in the
// palette. PaletteTray has one instance per-display. It is only made visible if
// the display is primary and if the device has stylus hardware.
class ASH_EXPORT PaletteTray : public TrayBackgroundView,
                               public SessionObserver,
                               public ShellObserver,
                               public PaletteToolManager::Delegate,
                               public ui::InputDeviceEventObserver {
 public:
  // For testing.
  class TestApi {
   public:
    explicit TestApi(PaletteTray* palette_tray);
    ~TestApi();

    PaletteToolManager* GetPaletteToolManager() {
      return palette_tray_->palette_tool_manager_.get();
    }

    TrayBubbleWrapper* GetTrayBubbleWrapper() {
      return palette_tray_->bubble_.get();
    }

    bool IsStylusWatcherActive() { return !!palette_tray_->watcher_; }

   private:
    PaletteTray* palette_tray_ = nullptr;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  explicit PaletteTray(Shelf* shelf);
  ~PaletteTray() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ShellObserver:
  void OnLockStateChanged(bool locked) override;
  void OnLocalStatePrefServiceInitialized(PrefService* pref_service) override;

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void Initialize() override;
  bool PerformAction(const ui::Event& event) override;
  void CloseBubble() override;
  void ShowBubble(bool show_by_click) override;
  views::TrayBubbleView* GetBubbleView() override;

  // PaletteToolManager::Delegate:
  void HidePalette() override;
  void HidePaletteImmediately() override;
  void RecordPaletteOptionsUsage(PaletteTrayOptions option,
                                 PaletteInvocationMethod method) override;
  void RecordPaletteModeCancellation(PaletteModeCancelType type) override;

  // Returns true if the palette tray contains the given point. This is useful
  // for determining if an event should be propagated through to the palette.
  bool ContainsPointInScreen(const gfx::Point& point);

 private:
  class StylusWatcher;

  // ui::InputDeviceObserver:
  void OnTouchscreenDeviceConfigurationChanged() override;
  void OnStylusStateChanged(ui::StylusState stylus_state) override;

  // views::TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  void OnMouseEnteredView() override;
  void OnMouseExitedView() override;
  base::string16 GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const views::TrayBubbleView* bubble_view) override;

  // PaletteToolManager::Delegate:
  void OnActiveToolChanged() override;
  aura::Window* GetWindow() override;

  // Updates the tray icon from the palette tool manager.
  void UpdateTrayIcon();

  // Sets the icon to visible if the palette can be used.
  void UpdateIconVisibility();

  // Called when the palette enabled pref has changed.
  void OnPaletteEnabledPrefChanged(bool enabled);

  // Called when the has seen stylus pref has changed.
  void OnHasSeenStylusPrefChanged();

  // Deactivates the active tool. Returns false if there was no active tool.
  bool DeactivateActiveTool();

  std::unique_ptr<PaletteToolManager> palette_tool_manager_;
  std::unique_ptr<TrayBubbleWrapper> bubble_;
  std::unique_ptr<StylusWatcher> watcher_;

  // Manages the callback OnPaletteEnabledPrefChanged callback registered to
  // the PaletteDelegate instance.
  std::unique_ptr<PaletteDelegate::EnableListenerSubscription>
      palette_enabled_subscription_;

  PrefService* local_state_pref_service_ = nullptr;  // Not owned.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  // Cached palette pref values.
  bool is_palette_enabled_ = true;
  bool has_seen_stylus_ = false;

  // Used to indicate whether the palette bubble is automatically opened by a
  // stylus eject event.
  bool is_bubble_auto_opened_ = false;

  // Number of actions in pen palette bubble.
  int num_actions_in_bubble_ = 0;

  ScopedSessionObserver scoped_session_observer_;

  base::WeakPtrFactory<PaletteTray> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PaletteTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_PALETTE_TRAY_H_
