// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_H_

#include <string>

#include "base/basictypes.h"
#include "base/process_util.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

struct WebDropData;

namespace content {

// The WebContentsView is an interface that is implemented by the platform-
// dependent web contents views. The WebContents uses this interface to talk to
// them.
class CONTENT_EXPORT WebContentsView {
 public:
  virtual ~WebContentsView() {}

  // Returns the native widget that contains the contents of the tab.
  virtual gfx::NativeView GetNativeView() const = 0;

  // Returns the native widget with the main content of the tab (i.e. the main
  // render view host, though there may be many popups in the tab as children of
  // the container).
  virtual gfx::NativeView GetContentNativeView() const = 0;

  // Returns the outermost native view. This will be used as the parent for
  // dialog boxes.
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const = 0;

  // Computes the rectangle for the native widget that contains the contents of
  // the tab in the screen coordinate system.
  virtual void GetContainerBounds(gfx::Rect* out) const = 0;

  // Helper function for GetContainerBounds. Most callers just want to know the
  // size, and this makes it more clear.
  gfx::Size GetContainerSize() const {
    gfx::Rect rc;
    GetContainerBounds(&rc);
    return gfx::Size(rc.width(), rc.height());
  }

  // Used to notify the view that a tab has crashed.
  virtual void OnTabCrashed(base::TerminationStatus status, int error_code) = 0;

  // TODO(brettw) this is a hack. It's used in two places at the time of this
  // writing: (1) when render view hosts switch, we need to size the replaced
  // one to be correct, since it wouldn't have known about sizes that happened
  // while it was hidden; (2) in constrained windows.
  //
  // (1) will be fixed once interstitials are cleaned up. (2) seems like it
  // should be cleaned up or done some other way, since this works for normal
  // WebContents without the special code.
  virtual void SizeContents(const gfx::Size& size) = 0;

  // Sets focus to the native widget for this tab.
  virtual void Focus() = 0;

  // Sets focus to the appropriate element when the WebContents is shown the
  // first time.
  virtual void SetInitialFocus() = 0;

  // Stores the currently focused view.
  virtual void StoreFocus() = 0;

  // Restores focus to the last focus view. If StoreFocus has not yet been
  // invoked, SetInitialFocus is invoked.
  virtual void RestoreFocus() = 0;

  // Returns the current drop data, if any.
  virtual WebDropData* GetDropData() const = 0;

  // Get the bounds of the View, relative to the parent.
  virtual gfx::Rect GetViewBounds() const = 0;

#if defined(OS_MACOSX)
  // The web contents view assumes that its view will never be overlapped by
  // another view (either partially or fully). This allows it to perform
  // optimizations. If the view is in a view hierarchy where it might be
  // overlapped by another view, notify the view by calling this with |true|
  // before it draws for the first time. After the first draw, do not change
  // this setting.
  virtual void SetAllowOverlappingViews(bool overlapping) = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_H_
