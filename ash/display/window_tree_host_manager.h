// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CONTROLLER_H_
#define ASH_DISPLAY_DISPLAY_CONTROLLER_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "ash/display/display_manager.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Display;
class WindowTreeHost;
}

namespace base {
class Value;
template <typename T>
class JSONValueConverter;
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
class InputMethodEventHandler;
class MirrorWindowController;
class RootWindowController;

// WindowTreeHostManager owns and maintains RootWindows for each attached
// display, keeping them in sync with display configuration changes.
class ASH_EXPORT WindowTreeHostManager
    : public gfx::DisplayObserver,
      public aura::WindowTreeHostObserver,
      public DisplayManager::Delegate,
      public ui::internal::InputMethodDelegate {
 public:
  // TODO(oshima): Consider moving this to gfx::DisplayObserver.
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
    virtual void OnDisplayConfigurationChanged() {}

    // Invoked in WindowTreeHostManager::Shutdown().
    virtual void OnWindowTreeHostManagerShutdown() {}

   protected:
    virtual ~Observer() {}
  };

  WindowTreeHostManager();
  ~WindowTreeHostManager() override;

  void Start();
  void Shutdown();

  // Returns primary display's ID.
  // TODO(oshima): Move this out from WindowTreeHostManager;
  static int64_t GetPrimaryDisplayId();

  CursorWindowController* cursor_window_controller() {
    return cursor_window_controller_.get();
  }

  MirrorWindowController* mirror_window_controller() {
    return mirror_window_controller_.get();
  }

  // Create a WindowTreeHost for the primary display. This replaces
  // |initial_bounds| in |init_params|.
  void CreatePrimaryHost(const AshWindowTreeHostInitParams& init_params);

  // Initializes all WindowTreeHosts.
  void InitHosts();

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the root window for primary display.
  aura::Window* GetPrimaryRootWindow();

  // Returns the root window for |display_id|.
  aura::Window* GetRootWindowForDisplayId(int64_t id);

  // Returns AshWTH for given display |id|. Call results in CHECK failure
  // if the WTH does not exist.
  AshWindowTreeHost* GetAshWindowTreeHostForDisplayId(int64_t id);

  // Sets the ID of the primary display.  If the display is not connected, it
  // will switch the primary display when connected.
  void SetPrimaryDisplayId(int64_t id);

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
  gfx::Insets GetOverscanInsets(int64_t display_id) const;
  void SetOverscanInsets(int64_t display_id, const gfx::Insets& insets_in_dip);

  // Checks if the mouse pointer is on one of displays, and moves to
  // the center of the nearest display if it's outside of all displays.
  void UpdateMouseLocationAfterDisplayChange();

  // Sets the work area's |insets| to the display assigned to |window|.
  bool UpdateWorkAreaOfDisplayNearestWindow(const aura::Window* window,
                                            const gfx::Insets& insets);

  ui::InputMethod* input_method() { return input_method_.get(); }

  // gfx::DisplayObserver overrides:
  void OnDisplayAdded(const gfx::Display& display) override;
  void OnDisplayRemoved(const gfx::Display& display) override;
  void OnDisplayMetricsChanged(const gfx::Display& display,
                               uint32_t metrics) override;

  // aura::WindowTreeHostObserver overrides:
  void OnHostResized(const aura::WindowTreeHost* host) override;

  // aura::DisplayManager::Delegate overrides:
  void CreateOrUpdateMirroringDisplay(
      const DisplayInfoList& info_list) override;
  void CloseMirroringDisplayIfNotNecessary() override;
  void PreDisplayConfigurationChange(bool clear_focus) override;
  void PostDisplayConfigurationChange() override;

  // ui::internal::InputMethodDelegate overrides:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override;

  InputMethodEventHandler* input_method_event_handler() {
    return input_method_event_handler_.get();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WindowTreeHostManagerTest, BoundsUpdated);
  FRIEND_TEST_ALL_PREFIXES(WindowTreeHostManagerTest, SecondaryDisplayLayout);
  friend class DisplayManager;
  friend class MirrorWindowController;

  // Creates a WindowTreeHost for |display| and stores it in the
  // |window_tree_hosts_| map.
  AshWindowTreeHost* AddWindowTreeHostForDisplay(
      const gfx::Display& display,
      const AshWindowTreeHostInitParams& params);

  // Delete the AsWindowTreeHost. This does not remove the entry from
  // |window_tree_hosts_|. Caller has to explicitly remove it.
  void DeleteHost(AshWindowTreeHost* host_to_delete);

  typedef std::map<int64_t, AshWindowTreeHost*> WindowTreeHostMap;
  // The mapping from display ID to its window tree host.
  WindowTreeHostMap window_tree_hosts_;

  base::ObserverList<Observer, true> observers_;

  // Store the primary window tree host temporarily while replacing
  // display.
  AshWindowTreeHost* primary_tree_host_for_replace_;

  scoped_ptr<FocusActivationStore> focus_activation_store_;

  scoped_ptr<CursorWindowController> cursor_window_controller_;
  scoped_ptr<MirrorWindowController> mirror_window_controller_;

  scoped_ptr<ui::InputMethod> input_method_;
  scoped_ptr<InputMethodEventHandler> input_method_event_handler_;

  // Stores the current cursor location (in native coordinates and screen
  // coordinates respectively). The locations are used to restore the cursor
  // location when the display configuration changes and to determine whether
  // the mouse should be moved after a display configuration change.
  gfx::Point cursor_location_in_native_coords_for_restore_;
  gfx::Point cursor_location_in_screen_coords_for_restore_;

  // Stores the cursor's display. The id is used to determine whether the mouse
  // should be moved after a display configuration change.
  int64_t cursor_display_id_for_restore_;

  base::WeakPtrFactory<WindowTreeHostManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostManager);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CONTROLLER_H_
