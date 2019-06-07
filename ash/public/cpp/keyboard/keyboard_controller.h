// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_CONTROLLER_H_
#define ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "ash/public/cpp/keyboard/keyboard_types.h"
#include "base/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

enum class HideReason {
  // Hide requested by an explicit user action.
  kUser,

  // Hide requested due to a system event (e.g. because it would interfere with
  // a menu or other on screen UI).
  kSystem,
};

class KeyboardControllerObserver {
 public:
  virtual ~KeyboardControllerObserver() {}

  // Called when a keyboard enable flag changes.
  virtual void OnKeyboardEnableFlagsChanged(
      const std::vector<keyboard::KeyboardEnableFlag>& flags) = 0;

  // Called when the keyboard is enabled or disabled. If ReloadKeyboard() is
  // called or other code enables the keyboard while already enabled, this will
  // be called twice, once when the keyboard is disabled and again when it is
  // re-enabled.
  virtual void OnKeyboardEnabledChanged(bool is_enabled) = 0;

  // Called when the virtual keyboard configuration changes.
  virtual void OnKeyboardConfigChanged(
      const keyboard::KeyboardConfig& config) = 0;

  // Called when the visibility of the virtual keyboard changes, e.g. an input
  // field is focused or blurred, or the user hides the keyboard.
  virtual void OnKeyboardVisibilityChanged(bool visible) = 0;

  // Called when the keyboard bounds change. |screen_bounds| is in screen
  // coordinates.
  virtual void OnKeyboardVisibleBoundsChanged(
      const gfx::Rect& screen_bounds) = 0;

  // Called when the keyboard occluded bounds change. |screen_bounds| is in
  // screen coordinates.
  virtual void OnKeyboardOccludedBoundsChanged(
      const gfx::Rect& screen_bounds) = 0;

  // Signals a request to load the keyboard contents. If the contents are
  // already loaded, requests a reload. Once the contents have loaded,
  // KeyboardController.KeyboardContentsLoaded is expected to be called by the
  // client implementation.
  virtual void OnLoadKeyboardContentsRequested() = 0;

  // Called when the UI has been destroyed so that the client can reset the
  // embedded contents and handle.
  virtual void OnKeyboardUIDestroyed() = 0;
};

class ASH_PUBLIC_EXPORT KeyboardController {
 public:
  using GetKeyboardConfigCallback =
      base::OnceCallback<void(const keyboard::KeyboardConfig&)>;
  using IsKeyboardEnabledCallback = base::OnceCallback<void(bool)>;
  using GetEnableFlagsCallback = base::OnceCallback<void(
      const std::vector<keyboard::KeyboardEnableFlag>&)>;
  using IsKeyboardVisibleCallback = base::OnceCallback<void(bool)>;
  using SetContainerTypeCallback = base::OnceCallback<void(bool)>;

  static KeyboardController* Get();

  // Sets the global KeyboardController instance to |this|.
  KeyboardController();

  virtual ~KeyboardController();

  // Informs the controller that the keyboard contents have loaded.
  virtual void KeyboardContentsLoaded(const gfx::Size& size) = 0;

  // Retrieves the current keyboard configuration.
  virtual void GetKeyboardConfig(GetKeyboardConfigCallback callback) = 0;

  // Sets the current keyboard configuration.
  virtual void SetKeyboardConfig(const keyboard::KeyboardConfig& config) = 0;

  // Returns whether the virtual keyboard has been enabled.
  virtual void IsKeyboardEnabled(IsKeyboardVisibleCallback callback) = 0;

  // Sets the provided keyboard enable flag. If the computed enabled state
  // changes, enables or disables the keyboard to match the new state.
  virtual void SetEnableFlag(keyboard::KeyboardEnableFlag flag) = 0;

  // Clears the provided keyboard enable flag. If the computed enabled state
  // changes, enables or disables the keyboard to match the new state.
  virtual void ClearEnableFlag(keyboard::KeyboardEnableFlag flag) = 0;

  // Gets the current set of keyboard enable flags.
  virtual void GetEnableFlags(GetEnableFlagsCallback callback) = 0;

  // Reloads the virtual keyboard if it is enabled and the URL has changed, e.g.
  // the focus has switched from one type of field to another.
  virtual void ReloadKeyboardIfNeeded() = 0;

  // Rebuilds (disables and re-enables) the virtual keyboard if it is enabled.
  // This is used to force a reload of the virtual keyboard when preferences or
  // other configuration that affects loading the keyboard may have changed.
  virtual void RebuildKeyboardIfEnabled() = 0;

  // Returns whether the virtual keyboard is visible.
  virtual void IsKeyboardVisible(IsKeyboardVisibleCallback callback) = 0;

  // Shows the virtual keyboard on the current display if it is enabled.
  virtual void ShowKeyboard() = 0;

  // Hides the virtual keyboard if it is visible.
  virtual void HideKeyboard(HideReason reason) = 0;

  // Sets the keyboard container type. If non empty, |target_bounds| provides
  // the container size. Returns whether the transition succeeded once the
  // container type changes (or fails to change).
  virtual void SetContainerType(keyboard::ContainerType container_type,
                                const base::Optional<gfx::Rect>& target_bounds,
                                SetContainerTypeCallback callback) = 0;

  // If |locked| is true, the keyboard remains visible even when no window has
  // input focus.
  virtual void SetKeyboardLocked(bool locked) = 0;

  // Sets the regions of the keyboard window that occlude whatever is behind it.
  virtual void SetOccludedBounds(const std::vector<gfx::Rect>& bounds) = 0;

  // Sets the regions of the keyboard window where events should be handled.
  virtual void SetHitTestBounds(const std::vector<gfx::Rect>& bounds) = 0;

  // Sets the region of the keyboard window that can be used as a drag handle.
  virtual void SetDraggableArea(const gfx::Rect& bounds) = 0;

  // Adds a KeyboardControllerObserver.
  virtual void AddObserver(KeyboardControllerObserver* observer) = 0;

 protected:
  static KeyboardController* g_instance_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_CONTROLLER_H_
