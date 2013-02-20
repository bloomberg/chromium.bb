// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_

#include "base/basictypes.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/gfx/native_widget_types.h"

#if defined(TOOLKIT_GTK)
#include <gdk/gdk.h>
#endif

class GURL;

namespace gfx {
class Rect;
class Size;
}

namespace content {

class BrowserAccessibilityManager;
class RenderWidgetHost;

// RenderWidgetHostView is an interface implemented by an object that acts as
// the "View" portion of a RenderWidgetHost. The RenderWidgetHost and its
// associated RenderProcessHost own the "Model" in this case which is the
// child renderer process. The View is responsible for receiving events from
// the surrounding environment and passing them to the RenderWidgetHost, and
// for actually displaying the content of the RenderWidgetHost when it
// changes.
//
// RenderWidgetHostView Class Hierarchy:
//   RenderWidgetHostView - Public interface.
//   RenderWidgetHostViewPort - Private interface for content/ and ports.
//   RenderWidgetHostViewBase - Common implementation between platforms.
//   RenderWidgetHostViewWin, ... - Platform specific implementations.
class CONTENT_EXPORT RenderWidgetHostView {
 public:
  virtual ~RenderWidgetHostView() {}

  // Platform-specific creator. Use this to construct new RenderWidgetHostViews
  // rather than using RenderWidgetHostViewWin & friends.
  //
  // This function must NOT size it, because the RenderView in the renderer
  // wouldn't have been created yet. The widget would set its "waiting for
  // resize ack" flag, and the ack would never come becasue no RenderView
  // received it.
  //
  // The RenderWidgetHost must already be created (because we can't know if it's
  // going to be a regular RenderWidgetHost or a RenderViewHost (a subclass).
  static RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* widget);

  // Initialize this object for use as a drawing area.  |parent_view| may be
  // left as NULL on platforms where a parent view is not required to initialize
  // a child window.
  virtual void InitAsChild(gfx::NativeView parent_view) = 0;

  // Returns the associated RenderWidgetHost.
  virtual RenderWidgetHost* GetRenderWidgetHost() const = 0;

  // Tells the View to size itself to the specified size.
  virtual void SetSize(const gfx::Size& size) = 0;

  // Tells the View to size and move itself to the specified size and point in
  // screen space.
  virtual void SetBounds(const gfx::Rect& rect) = 0;

  // Retrieves the native view used to contain plugins and identify the
  // renderer in IPC messages.
  virtual gfx::NativeView GetNativeView() const = 0;
  virtual gfx::NativeViewId GetNativeViewId() const = 0;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() = 0;

  // Set focus to the associated View component.
  virtual void Focus() = 0;
  // Returns true if the View currently has the focus.
  virtual bool HasFocus() const = 0;
  // Returns true is the current display surface is available.
  virtual bool IsSurfaceAvailableForCopy() const = 0;

  // Shows/hides the view.  These must always be called together in pairs.
  // It is not legal to call Hide() multiple times in a row.
  virtual void Show() = 0;
  virtual void Hide() = 0;

  // Whether the view is showing.
  virtual bool IsShowing() = 0;

  // Retrieve the bounds of the View, in screen coordinates.
  virtual gfx::Rect GetViewBounds() const = 0;

  // Returns true if the View's context menu is showing.
  virtual bool IsShowingContextMenu() const = 0;

  // Tells the View whether the context menu is showing.
  virtual void SetShowingContextMenu(bool showing) = 0;

  // Returns the currently selected text.
  virtual string16 GetSelectedText() const = 0;

#if defined(OS_MACOSX)
  // Set the view's active state (i.e., tint state of controls).
  virtual void SetActive(bool active) = 0;

  // Tells the view whether or not to accept first responder status.  If |flag|
  // is true, the view does not accept first responder status and instead
  // manually becomes first responder when it receives a mouse down event.  If
  // |flag| is false, the view participates in the key-view chain as normal.
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) = 0;

  // Notifies the view that its enclosing window has changed visibility
  // (minimized/unminimized, app hidden/unhidden, etc).
  // TODO(stuartmorgan): This is a temporary plugin-specific workaround for
  // <http://crbug.com/34266>. Once that is fixed, this (and the corresponding
  // message and renderer-side handling) can be removed in favor of using
  // WasHidden/WasShown.
  virtual void SetWindowVisibility(bool visible) = 0;

  // Informs the view that its containing window's frame changed.
  virtual void WindowFrameChanged() = 0;

  // Brings up the dictionary showing a definition for the selected text.
  virtual void ShowDefinitionForSelection() = 0;

  // Returns |true| if Mac OS X text to speech is supported.
  virtual bool SupportsSpeech() const = 0;
  // Tells the view to speak the currently selected text.
  virtual void SpeakSelection() = 0;
  // Returns |true| if text is currently being spoken by Mac OS X.
  virtual bool IsSpeaking() const = 0;
  // Stops speaking, if it is currently in progress.
  virtual void StopSpeaking() = 0;
#endif  // defined(OS_MACOSX)

#if defined(TOOLKIT_GTK)
  // Gets the event for the last mouse down.
  virtual GdkEventButton* GetLastMouseDown() = 0;
  // Builds a submenu containing all the gtk input method commands.
  virtual gfx::NativeView BuildInputMethodsGtkMenu() = 0;
#endif  // defined(TOOLKIT_GTK)

#if defined(OS_ANDROID)
  virtual void StartContentIntent(const GURL& content_url) = 0;
  virtual void SetCachedBackgroundColor(SkColor color) = 0;
#endif

  // Subclasses should override this method to do what is appropriate to set
  // the custom background for their platform.
  virtual void SetBackground(const SkBitmap& background) = 0;
  virtual const SkBitmap& GetBackground() = 0;

#if defined(OS_WIN) && !defined(USE_AURA)
  // The region specified will be transparent to mouse clicks.
  virtual void SetClickthroughRegion(SkRegion* region) = 0;
#endif

  // Return value indicates whether the mouse is locked successfully or not.
  virtual bool LockMouse() = 0;
  virtual void UnlockMouse() = 0;
  // Returns true if the mouse pointer is currently locked.
  virtual bool IsMouseLocked() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_
