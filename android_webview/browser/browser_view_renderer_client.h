// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_CLIENT_H_

#include "base/android/scoped_java_ref.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace android_webview {

class BrowserViewRendererClient {
 public:
  // Request DrawGL be called. Passing null |canvas| implies the request
  // will be of AwDrawGLInfo::kModeProcess type. |wait_for_completion|
  // will cause the call to block until DrawGL has happened; it does not
  // work when |canvas| is not NULL. The callback may never be made, and
  // the mode may be promoted to kModeDraw.
  virtual bool RequestDrawGL(jobject canvas, bool wait_for_completion) = 0;

  // Called when a new Picture is available. Needs to be enabled
  // via the EnableOnNewPicture method.
  virtual void OnNewPicture() = 0;

  // Called to trigger view invalidations.
  virtual void PostInvalidate() = 0;

  // Called to update the parent draw constraints in browser view renderer.
  virtual void UpdateParentDrawConstraints() = 0;

  // Called to get view's absolute location on the screen.
  virtual gfx::Point GetLocationOnScreen() = 0;

  // Try to set the view's scroll offset to |new_value|.
  virtual void ScrollContainerViewTo(gfx::Vector2d new_value) = 0;

  // Is a Android view system managed fling in progress?
  virtual bool IsFlingActive() const = 0;

  // Sets the following:
  // view's scroll offset cap to |max_scroll_offset|,
  // current contents_size to |contents_size_dip|,
  // the current page scale to |page_scale_factor| and page scale limits
  // to |min_page_scale_factor|..|max_page_scale_factor|.
  virtual void UpdateScrollState(gfx::Vector2d max_scroll_offset,
                                 gfx::SizeF contents_size_dip,
                                 float page_scale_factor,
                                 float min_page_scale_factor,
                                 float max_page_scale_factor) = 0;

  // Handle overscroll.
  virtual void DidOverscroll(gfx::Vector2d overscroll_delta) = 0;

 protected:
  virtual ~BrowserViewRendererClient() {}
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_CLIENT_H_
