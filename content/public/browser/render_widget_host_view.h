// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_

#include <memory>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/readback_types.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace media {
class VideoFrame;
}

namespace ui {
class AcceleratedWidgetMac;
class TextInputClient;
}

namespace content {

class RenderWidgetHost;
class RenderWidgetHostViewFrameSubscriber;
class TouchSelectionControllerClientManager;

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
//   RenderWidgetHostViewBase - Common implementation between platforms.
//   RenderWidgetHostViewAura, ... - Platform specific implementations.
class CONTENT_EXPORT RenderWidgetHostView {
 public:
  virtual ~RenderWidgetHostView() {}

  // Initialize this object for use as a drawing area.  |parent_view| may be
  // left as nullptr on platforms where a parent view is not required to
  // initialize a child window.
  virtual void InitAsChild(gfx::NativeView parent_view) = 0;

  // Returns the associated RenderWidgetHost.
  virtual RenderWidgetHost* GetRenderWidgetHost() const = 0;

  // Tells the View to size itself to the specified size.
  virtual void SetSize(const gfx::Size& size) = 0;

  // Tells the View to size and move itself to the specified size and point in
  // screen space.
  virtual void SetBounds(const gfx::Rect& rect) = 0;

  // Retrieves the last known scroll position.
  virtual gfx::Vector2dF GetLastScrollOffset() const = 0;

  // Sets a flag that indicates if it is in virtual reality mode.
  virtual void SetIsInVR(bool is_in_vr) = 0;

  // Coordinate points received from a renderer process need to be transformed
  // to the top-level frame's coordinate space. For coordinates received from
  // the top-level frame's renderer this is a no-op as they are already
  // properly transformed; however, coordinates received from an out-of-process
  // iframe renderer process require transformation.
  virtual gfx::PointF TransformPointToRootCoordSpaceF(
      const gfx::PointF& point) = 0;

  // A int point variant of the above. Use float version to transform,
  // then rounded back to int point.
  gfx::Point TransformPointToRootCoordSpace(const gfx::Point& point) {
    return gfx::ToRoundedPoint(
        TransformPointToRootCoordSpaceF(gfx::PointF(point)));
  }

  // Converts a point in the root view's coordinate space to the coordinate
  // space of whichever view is used to call this method.
  virtual gfx::PointF TransformRootPointToViewCoordSpace(
      const gfx::PointF& point) = 0;

  // Retrieves the native view used to contain plugins and identify the
  // renderer in IPC messages.
  virtual gfx::NativeView GetNativeView() const = 0;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() = 0;

  // Returns a ui::TextInputClient to support text input or nullptr if this RWHV
  // doesn't support text input.
  // Note: Not all the platforms use ui::InputMethod and ui::TextInputClient for
  // text input.  Some platforms (Mac and Android for example) use their own
  // text input system.
  virtual ui::TextInputClient* GetTextInputClient() = 0;

  // Set focus to the associated View component.
  virtual void Focus() = 0;
  // Returns true if the View currently has the focus.
  virtual bool HasFocus() const = 0;

  // Shows/hides the view.  These must always be called together in pairs.
  // It is not legal to call Hide() multiple times in a row.
  virtual void Show() = 0;
  virtual void Hide() = 0;

  // Whether the view is showing.
  virtual bool IsShowing() = 0;

  // Indicates if the view is currently occluded (e.g, not visible because it's
  // covered up by other windows), and as a result the view's renderer may be
  // suspended. If Show() is called on a view then its state should be re-set to
  // being un-occluded (an explicit WasUnOccluded call will not be made for
  // that). These calls are not necessarily made in pairs.
  virtual void WasUnOccluded() = 0;
  virtual void WasOccluded() = 0;

  // Retrieve the bounds of the View, in screen coordinates.
  virtual gfx::Rect GetViewBounds() const = 0;

  // Returns the currently selected text.
  virtual base::string16 GetSelectedText() = 0;

  // This only returns non-null on platforms that implement touch
  // selection editing (TSE), currently Aura and (soon) Android.
  // TODO(wjmaclean): update this comment when OOPIF TSE is implemented on
  // Android. https://crbug.com/470662.
  virtual TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() = 0;

  // Subclasses should override this method to set the background color. |color|
  // has to be either SK_ColorTRANSPARENT or opaque. If set to
  // SK_ColorTRANSPARENT, the renderer's background color will be overridden to
  // be fully transparent.
  virtual void SetBackgroundColor(SkColor color) = 0;
  virtual SkColor background_color() const = 0;
  // Convenience method to fill the background layer with the default color by
  // calling |SetBackgroundColor|.
  virtual void SetBackgroundColorToDefault() = 0;

  // Return value indicates whether the mouse is locked successfully or not.
  virtual bool LockMouse() = 0;
  virtual void UnlockMouse() = 0;
  // Returns true if the mouse pointer is currently locked.
  virtual bool IsMouseLocked() = 0;

  // Retrives the size of the viewport for the visible region. May be smaller
  // than the view size if a portion of the view is obstructed (e.g. by a
  // virtual keyboard).
  virtual gfx::Size GetVisibleViewportSize() const = 0;

  // Set insets for the visible region of the root window. Used to compute the
  // visible viewport.
  virtual void SetInsets(const gfx::Insets& insets) = 0;

  // Returns true if the current display surface is available, a prerequisite
  // for CopyFromSurface() or CopyFromSurfaceToVideoFrame() to succeed.
  virtual bool IsSurfaceAvailableForCopy() const = 0;

  // Copies the given subset of the view's surface, optionally scales it, and
  // returns the result as a bitmap via the provided callback. This is meant for
  // one-off snapshots. For continuous video capture of the surface, please use
  // BeginFrameSubscription().
  //
  // |src_rect| is either the subset of the view's surface, in view coordinates,
  // or empty to indicate that all of it should be copied. This is NOT the same
  // coordinate system as that used GetViewBounds() (https://crbug.com/73362).
  //
  // |output_size| is the size of the resulting bitmap, or empty to indicate no
  // scaling is desired. If an empty size is provided, note that the resulting
  // bitmap's size may not be the same as |src_rect.size()| due to the pixel
  // scale used by the underlying device.
  //
  // |callback| is always invoked, at some point in the future, with success/
  // fail status and an SkBitmap containing the copied pixel data. It may be
  // called sychronously or asynchronously.
  //
  // If the view's renderer is suspended (see WasOccluded()), this may result in
  // copying old data or failing.
  virtual void CopyFromSurface(const gfx::Rect& src_rect,
                               const gfx::Size& output_size,
                               const ReadbackRequestCallback& callback,
                               const SkColorType color_type) = 0;

  // Copies the given subset of the view's surface, scales/letterboxes it, and
  // returns the result within the provided media::VideoFrame. This is meant for
  // one-off snapshots. For continuous video capture of the surface, please use
  // BeginFrameSubscription().
  //
  // |src_rect| is either the subset of the view's surface, in view coordinates,
  // or empty to indicate that all of it should be copied. This is NOT the same
  // coordinate system as that used GetViewBounds() (https://crbug.com/73362).
  //
  // The |target| must use a pixel format supported by the platform. The size of
  // the source region and the target together will determine how the content is
  // positioned and scaled within the video frame. The aspect ratio of the
  // source region will always be preserved, with letterboxing/pillarboxing
  // applied to fill the entire frame.
  //
  // |callback| is always invoked, at some point in the future, with the region
  // within |target| where the content was placed and a boolean success/fail
  // flag. It may be called sychronously or asynchronously.
  //
  // If the view's renderer is suspended (see WasOccluded()), this may result in
  // copying old data or failing.
  virtual void CopyFromSurfaceToVideoFrame(
      const gfx::Rect& src_rect,
      scoped_refptr<media::VideoFrame> target,
      const base::Callback<void(const gfx::Rect&, bool)>& callback) = 0;

  // Begin subscribing for presentation events and captured frames.
  // |subscriber| is now owned by this object, it will be called only on the
  // UI thread.
  virtual void BeginFrameSubscription(
      std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) = 0;

  // End subscribing for frame presentation events. FrameSubscriber will be
  // deleted after this call.
  virtual void EndFrameSubscription() = 0;

  // Notification that a node was touched.
  // The |location_dips_screen| parameter contains the location where the touch
  // occurred in DIPs in screen coordinates.
  // The |editable| parameter indicates if the node is editable, for e.g.
  // an input field, etc.
  virtual void FocusedNodeTouched(const gfx::Point& location_dips_screen,
                                  bool editable) = 0;

  // Informs the view that its associated render widget has frames to draw and
  // wants to have BeginFrame messages sent to it.  This should only be called
  // when the value has changed.  Views must initially default to false.
  virtual void SetNeedsBeginFrames(bool needs_begin_frames) = 0;

#if defined(OS_MACOSX)
  // Return the accelerated widget which hosts the CALayers that draw the
  // content of the view in GetNativeView. This may be null.
  virtual ui::AcceleratedWidgetMac* GetAcceleratedWidgetMac() const = 0;

  // Set the view's active state (i.e., tint state of controls).
  virtual void SetActive(bool active) = 0;

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
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_
