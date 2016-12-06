// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_WINDOW_MUS_H_
#define ASH_MUS_BRIDGE_WM_WINDOW_MUS_H_

#include <memory>

#include "ash/aura/wm_window_aura.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {
namespace mus {

class WmRootWindowControllerMus;
class WmWindowMusTestApi;

// WmWindow implementation for mus.
//
// WmWindowMus is tied to the life of the underlying aura::Window (it is stored
// as an owned property).
class WmWindowMus : public WmWindowAura {
 public:
  // Indicates the source of the widget creation.
  enum class WidgetCreationType {
    // The widget was created internally, and not at the request of a client.
    // For example, overview mode creates a number of widgets. These widgets are
    // created with a type of INTERNAL.
    INTERNAL,

    // The widget was created for a client. In other words there is a client
    // embedded in the aura::Window. For example, when Chrome creates a new
    // browser window the window manager is asked to create the aura::Window.
    // The window manager creates an aura::Window and a views::Widget to show
    // the non-client frame decorations. In this case the creation type is
    // FOR_CLIENT.
    FOR_CLIENT,
  };

  explicit WmWindowMus(aura::Window* window);
  // NOTE: this class is owned by the corresponding window. You shouldn't delete
  // TODO(sky): friend deleter and make private.
  ~WmWindowMus() override;

  // Returns a WmWindow for an aura::Window, creating if necessary.
  static WmWindowMus* Get(aura::Window* window) {
    return const_cast<WmWindowMus*>(
        Get(const_cast<const aura::Window*>(window)));
  }
  static const WmWindowMus* Get(const aura::Window* window);

  static WmWindowMus* Get(views::Widget* widget);

  // Sets the widget associated with the window. The widget is used to query
  // state, such as min/max size. The widget is not owned by the WmWindowMus.
  void set_widget(views::Widget* widget, WidgetCreationType type) {
    widget_ = widget;
    widget_creation_type_ = type;
  }

  WmRootWindowControllerMus* GetRootWindowControllerMus() {
    return const_cast<WmRootWindowControllerMus*>(
        const_cast<const WmWindowMus*>(this)->GetRootWindowControllerMus());
  }
  const WmRootWindowControllerMus* GetRootWindowControllerMus() const;

  static WmWindowMus* AsWmWindowMus(WmWindow* window) {
    return static_cast<WmWindowMus*>(window);
  }
  static const WmWindowMus* AsWmWindowMus(const WmWindow* window) {
    return static_cast<const WmWindowMus*>(window);
  }

  // See description of |children_use_extended_hit_region_|.
  bool ShouldUseExtendedHitRegion() const;

  // Returns true if this window is considered a shell window container.
  bool IsContainer() const;

  // WmWindow:
  const WmWindow* GetRootWindow() const override;
  WmRootWindowController* GetRootWindowController() override;
  WmShell* GetShell() const override;
  bool IsBubble() override;
  bool HasNonClientArea() override;
  int GetNonClientComponent(const gfx::Point& location) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  gfx::Rect GetMinimizeAnimationTargetBoundsInScreen() const override;
  bool IsSystemModal() const override;
  bool GetBoolProperty(WmWindowProperty key) override;
  int GetIntProperty(WmWindowProperty key) override;
  WmWindow* GetToplevelWindow() override;
  WmWindow* GetToplevelWindowForFocus() override;
  bool MoveToEventRoot(const ui::Event& event) override;
  void SetBoundsInScreen(const gfx::Rect& bounds_in_screen,
                         const display::Display& dst_display) override;
  void SetPinned(bool trusted) override;
  views::Widget* GetInternalWidget() override;
  void CloseWidget() override;
  bool CanActivate() const override;
  void ShowResizeShadow(int component) override;
  void HideResizeShadow() override;
  void InstallResizeHandleWindowTargeter(
      ImmersiveFullscreenController* immersive_fullscreen_controller) override;
  void SetBoundsInScreenBehaviorForChildren(
      BoundsInScreenBehavior behavior) override;
  void SetSnapsChildrenToPhysicalPixelBoundary() override;
  void SnapToPixelBoundaryIfNecessary() override;
  void SetChildrenUseExtendedHitRegion() override;
  void AddLimitedPreTargetHandler(ui::EventHandler* handler) override;

 private:
  friend class WmWindowMusTestApi;

  views::Widget* widget_ = nullptr;

  WidgetCreationType widget_creation_type_ = WidgetCreationType::INTERNAL;

  bool snap_children_to_pixel_boundary_ = false;

  // If true child windows should get a slightly larger hit region to make
  // resizing easier.
  bool children_use_extended_hit_region_ = false;

  // Default value for |use_empty_minimum_size_for_testing_|.
  static bool default_use_empty_minimum_size_for_testing_;

  // If true the minimum size is 0x0, default is minimum size comes from widget.
  bool use_empty_minimum_size_for_testing_ = false;

  BoundsInScreenBehavior child_bounds_in_screen_behavior_ =
      BoundsInScreenBehavior::USE_LOCAL_COORDINATES;

  DISALLOW_COPY_AND_ASSIGN(WmWindowMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_WINDOW_MUS_H_
