// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CONTROLLER_H_
#define ASH_DISPLAY_DISPLAY_CONTROLLER_H_

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "ash/display/display_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/point.h"

namespace aura {
class Display;
class WindowTreeHost;
}

namespace base {
class Value;
template <typename T> class JSONValueConverter;
}

namespace gfx {
class Display;
class Insets;
}

namespace ash {
class AshWindowTreeHost;
struct AshWindowTreeHostInitParams;
class CursorWindowController;
class DisplayInfo;
class DisplayManager;
class FocusActivationStore;
class MirrorWindowController;
class RootWindowController;
class VirtualKeyboardWindowController;

// DisplayController owns and maintains RootWindows for each attached
// display, keeping them in sync with display configuration changes.
class ASH_EXPORT DisplayController : public gfx::DisplayObserver,
                                     public aura::WindowTreeHostObserver,
                                     public DisplayManager::Delegate {
 public:
  class ASH_EXPORT Observer {
   public:
    // Invoked only once after all displays are initialized
    // after startup.
    virtual void OnDisplaysInitialized() {}

    // Invoked when the display configuration change is requested,
    // but before the change is applied to aura/ash.
    virtual void OnDisplayConfigurationChanging() {}

    // Invoked when the all display configuration changes
    // have been applied.
    virtual void OnDisplayConfigurationChanged() {};

   protected:
    virtual ~Observer() {}
  };

  DisplayController();
  virtual ~DisplayController();

  void Start();
  void Shutdown();

  // Returns primary display's ID.
  // TODO(oshima): Move this out from DisplayController;
  static int64 GetPrimaryDisplayId();

  CursorWindowController* cursor_window_controller() {
    return cursor_window_controller_.get();
  }

  MirrorWindowController* mirror_window_controller() {
    return mirror_window_controller_.get();
  }

  VirtualKeyboardWindowController* virtual_keyboard_window_controller() {
    return virtual_keyboard_window_controller_.get();
  }

  // Create a WindowTreeHost for the primary display. This replaces
  // |initial_bounds| in |init_params|.
  void CreatePrimaryHost(const AshWindowTreeHostInitParams& init_params);

  // Initializes all displays.
  void InitDisplays();

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the root window for primary display.
  aura::Window* GetPrimaryRootWindow();

  // Returns the root window for |display_id|.
  aura::Window* GetRootWindowForDisplayId(int64 id);

  // Toggle mirror mode.
  void ToggleMirrorMode();

  // Swap primary and secondary display.
  void SwapPrimaryDisplay();

  // Sets the ID of the primary display.  If the display is not connected, it
  // will switch the primary display when connected.
  void SetPrimaryDisplayId(int64 id);

  // Sets primary display. This re-assigns the current root
  // window to given |display|.
  void SetPrimaryDisplay(const gfx::Display& display);

  // Closes all child windows in the all root windows.
  void CloseChildWindows();

  // Returns all root windows. In non extended desktop mode, this
  // returns the primary root window only.
  aura::Window::Windows GetAllRootWindows();

  // Returns all oot window controllers. In non extended desktop
  // mode, this return a RootWindowController for the primary root window only.
  std::vector<RootWindowController*> GetAllRootWindowControllers();

  // Gets/Sets/Clears the overscan insets for the specified |display_id|. See
  // display_manager.h for the details.
  gfx::Insets GetOverscanInsets(int64 display_id) const;
  void SetOverscanInsets(int64 display_id, const gfx::Insets& insets_in_dip);

  // Checks if the mouse pointer is on one of displays, and moves to
  // the center of the nearest display if it's outside of all displays.
  void EnsurePointerInDisplays();

  // Sets the work area's |insets| to the display assigned to |window|.
  bool UpdateWorkAreaOfDisplayNearestWindow(const aura::Window* window,
                                            const gfx::Insets& insets);
  // gfx::DisplayObserver overrides:
  virtual void OnDisplayAdded(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t metrics) OVERRIDE;

  // aura::WindowTreeHostObserver overrides:
  virtual void OnHostResized(const aura::WindowTreeHost* host) OVERRIDE;

  // aura::DisplayManager::Delegate overrides:
  virtual void CreateOrUpdateNonDesktopDisplay(const DisplayInfo& info)
      OVERRIDE;
  virtual void CloseNonDesktopDisplay() OVERRIDE;
  virtual void PreDisplayConfigurationChange(bool clear_focus) OVERRIDE;
  virtual void PostDisplayConfigurationChange() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(DisplayControllerTest, BoundsUpdated);
  FRIEND_TEST_ALL_PREFIXES(DisplayControllerTest, SecondaryDisplayLayout);
  friend class DisplayManager;
  friend class MirrorWindowController;

  // Creates a WindowTreeHost for |display| and stores it in the
  // |window_tree_hosts_| map.
  AshWindowTreeHost* AddWindowTreeHostForDisplay(
      const gfx::Display& display,
      const AshWindowTreeHostInitParams& params);

  void OnFadeOutForSwapDisplayFinished();

  void SetMirrorModeAfterAnimation(bool mirror);

  void UpdateHostWindowNames();

  class DisplayChangeLimiter {
   public:
    DisplayChangeLimiter();

    // Sets how long the throttling should last.
    void SetThrottleTimeout(int64 throttle_ms);

    bool IsThrottled() const;

   private:
    // The time when the throttling ends.
    base::Time throttle_timeout_;

    DISALLOW_COPY_AND_ASSIGN(DisplayChangeLimiter);
  };

  // The limiter to throttle how fast a user can
  // change the display configuration.
  scoped_ptr<DisplayChangeLimiter> limiter_;

  typedef std::map<int64, AshWindowTreeHost*> WindowTreeHostMap;
  // The mapping from display ID to its window tree host.
  WindowTreeHostMap window_tree_hosts_;

  ObserverList<Observer> observers_;

  // Store the primary window tree host temporarily while replacing
  // display.
  AshWindowTreeHost* primary_tree_host_for_replace_;

  scoped_ptr<FocusActivationStore> focus_activation_store_;

  scoped_ptr<CursorWindowController> cursor_window_controller_;
  scoped_ptr<MirrorWindowController> mirror_window_controller_;
  scoped_ptr<VirtualKeyboardWindowController>
      virtual_keyboard_window_controller_;

  // Stores the curent cursor location (in native coordinates) used to
  // restore the cursor location when display configuration
  // changed.
  gfx::Point cursor_location_in_native_coords_for_restore_;

  base::WeakPtrFactory<DisplayController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplayController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CONTROLLER_H_
