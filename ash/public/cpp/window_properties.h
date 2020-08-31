// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_
#define ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_

#include <stdint.h>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/class_property.h"

class SkRegion;

namespace aura {
class Window;
template <typename T>
using WindowProperty = ui::ClassProperty<T>;
}  // namespace aura

namespace gfx {
class Rect;
}

namespace ash {

enum class WindowPinType;
enum class WindowStateType;

class WindowBackdrop;

// Shell-specific window property keys for use by ash and its clients.

// Alphabetical sort.

// A property key to store the app ID for the window's associated app.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<std::string*>* const
    kAppIDKey;

// A property key to store the ARC package name for a window's associated
// ARC app.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<std::string*>* const
    kArcPackageNameKey;

// A property key to specify whether the window should have backdrop and if
// it has backdrop, the backdrop's mode and type. The backdrop is typically a
// black window that covers the entire workspace placed behind the window.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<WindowBackdrop*>* const
    kWindowBackdropKey;

// If set to true, the window will be replaced by a black rectangle when taking
// screenshot for assistant. Used to preserve privacy for incognito windows.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kBlockedForAssistantSnapshotKey;

// If true, the window can attach into another window.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kCanAttachToAnotherWindowKey;

// If true, will send system keys to the window for dispatch.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kCanConsumeSystemKeysKey;

// A property key to exclude the window in MruTracker.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kExcludeInMruKey;

// A property key to indicate whether we should hide this window in overview
// mode and Alt + Tab.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kHideInOverviewKey;

// Whether the shelf should be hidden when this window is put into fullscreen.
// Exposed because some windows want to explicitly opt-out of this.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kHideShelfWhenFullscreenKey;

// Whether entering fullscreen means that a window should automatically enter
// immersive mode. This is false for some client windows, such as Chrome Apps.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kImmersiveImpliedByFullscreen;

// Whether immersive is currently active (in ImmersiveFullscreenController
// parlance, "enabled").
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kImmersiveIsActive;

// The bounds of the top container in screen coordinates.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<gfx::Rect*>* const
    kImmersiveTopContainerBoundsInScreen;

// The type of window for logging immersive metrics. Type:
// ImmersiveFullscreenController::WindowType.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<int>* const
    kImmersiveWindowType;

// If true, the window is the target window for the tab-dragged window. The key
// is used by overview to show a highlight indication to indicate which overview
// window the dragged tabs will merge into when the user releases the pointer.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kIsDeferredTabDraggingTargetWindowKey;

// If true, the window is a browser window and its tab(s) are currently being
// dragged.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kIsDraggingTabsKey;

// If true, the window is currently showing in overview mode.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kIsShowingInOverviewKey;

// If true, the window will be ignored when mirroring the desk contents into
// the desk's mini_view.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kHideInDeskMiniViewKey;

// If true, the mirror of the window in the mini_view will be forced to be
// visible and its visibility won't be synced with visibility changes of the
// source.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kForceVisibleInMiniViewKey;

// A property key to tell if the window's opacity should be managed by WM.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kWindowManagerManagesOpacityKey;

// A property key to store whether we should minimize a window when a system
// synthesized back event (back gesture, back button) is processed by this
// window and when this window is at the bottom of its navigation stack.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool*>* const
    kMinimizeOnBackKey;

// A property key to store the window state the window had before entering PIP.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<WindowStateType>* const
    kPrePipWindowStateTypeKey;

// If true, the current PIP window is spawned from this window.
// Android PIP has two types of behavior depending on how many activities the
// original task has before entering PIP.
// SAPIP(Single-activity PIP): If the original task has only one activity, PIP
// can be handled as window state change of the target window. In this case, the
// PIP original window is this exact PIP window.
// MAPIP(Multi-activity PIP): If the original task has more than one activities,
// a new window is created for PIP, which is a completely different one from
// the existing window. This existing window is the original window of the
// current PIP window in this case. This property is used, for example, to
// calculated the position of the PIP window in the Alt-Tab window cycler.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kPipOriginalWindowKey;

// A property key to store the PIP snap fraction for this window.
// The fraction is defined in a clockwise fashion against the PIP movement area.
//
//            0   1
//          4 +---+ 1
//            |   |
//          3 +---+ 2
//            3   2
//
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<float*>* const
    kPipSnapFractionKey;

// Maps to ws::mojom::WindowManager::kRenderParentTitleArea_Property.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kRenderTitleAreaProperty;

// A property key which stores the bounds in screen coordinates to restore a
// window to. These take preference over the current bounds. This is used by
// e.g. the tablet mode window manager.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<gfx::Rect*>* const
    kRestoreBoundsOverrideKey;

// A property key which stores the window state to restore a window to. These
// take preference over the current state if
// |kRestoreWindowStateTypeOverrideKey| is set. This is used by e.g. the tablet
// mode window manager.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<WindowStateType>* const
    kRestoreWindowStateTypeOverrideKey;

// A property key to store whether search key accelerator is reserved for a
// window. This is used to pass through search key accelerators to Android
// window if user is navigating with TalkBack (screen reader on Android).
// TalkBack uses search key as a modifier key.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kSearchKeyAcceleratorReservedKey;

// A property key to store the serialized id for a window's shelf item.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<std::string*>* const
    kShelfIDKey;

// A property key to store the type of a window's shelf item.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<int32_t>* const
    kShelfItemTypeKey;

// A property key to store the system gesture exclusion region. From a point
// inside the region, system gesture e.g. back gesture shouldn't be triggered.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<SkRegion*>* const
    kSystemGestureExclusionKey;

// A property key to store the address of the source window that the drag
// originated from if the window is currently in tab-dragging process.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<aura::Window*>* const
    kTabDraggingSourceWindowKey;

// A property key to store the active color on the window frame.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<SkColor>* const
    kFrameActiveColorKey;
// A property key to store the inactive color on the window frame.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<SkColor>* const
    kFrameInactiveColorKey;
// A property key that is set to true when the window frame should look like it
// is in restored state, but actually isn't. Set while dragging a maximized
// window.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kFrameRestoreLookKey;

// A property key whose value is shown in alt-tab/overview mode. If non-value
// is set, the window's title is used.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<base::string16*>* const
    kWindowOverviewTitleKey;

// A property key to store ash::WindowPinType for a window.
// When setting this property to PINNED or TRUSTED_PINNED, the window manager
// will try to fullscreen the window and pin it on the top of the screen. If the
// window manager failed to do it, the property will be restored to NONE. When
// setting this property to NONE, the window manager will restore the window.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<WindowPinType>* const
    kWindowPinTypeKey;

// A property key to indicate whether ash should perform auto management of
// window positions; when you open a second browser, ash will move the two to
// minimize overlap.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kWindowPositionManagedTypeKey;

// A property key to indicate ash's extended window state.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<WindowStateType>* const
    kWindowStateTypeKey;

// A property key to indicate pip window state.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kWindowPipTypeKey;

// Alphabetical sort.

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_
