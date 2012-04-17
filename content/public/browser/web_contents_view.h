// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace content {

class RenderViewHost;
class RenderWidgetHost;
class RenderWidgetHostView;

// The WebContentsView is an interface that is implemented by the platform-
// dependent web contents views. The WebContents uses this interface to talk to
// them. View-related messages will also get forwarded directly to this class
// from RenderViewHost via RenderViewHostDelegate::View.
class CONTENT_EXPORT WebContentsView
    : public content::RenderViewHostDelegate::View {
 public:
  WebContentsView() {}
  virtual ~WebContentsView() {}

  virtual void CreateView(const gfx::Size& initial_size) = 0;

  // Sets up the View that holds the rendered web page, receives messages for
  // it and contains page plugins. The host view should be sized to the current
  // size of the WebContents.
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) = 0;

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
  virtual void GetContainerBounds(gfx::Rect *out) const = 0;

  // Helper function for GetContainerBounds. Most callers just want to know the
  // size, and this makes it more clear.
  gfx::Size GetContainerSize() const {
    gfx::Rect rc;
    GetContainerBounds(&rc);
    return gfx::Size(rc.width(), rc.height());
  }

  // Sets the page title for the native widgets corresponding to the view. This
  // is not strictly necessary and isn't expected to be displayed anywhere, but
  // can aid certain debugging tools such as Spy++ on Windows where you are
  // trying to find a specific window.
  virtual void SetPageTitle(const string16& title) = 0;

  // Used to notify the view that a tab has crashed.
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) = 0;

  // TODO(brettw) this is a hack. It's used in two places at the time of this
  // writing: (1) when render view hosts switch, we need to size the replaced
  // one to be correct, since it wouldn't have known about sizes that happened
  // while it was hidden; (2) in constrained windows.
  //
  // (1) will be fixed once interstitials are cleaned up. (2) seems like it
  // should be cleaned up or done some other way, since this works for normal
  // WebContents without the special code.
  virtual void SizeContents(const gfx::Size& size) = 0;

  // Invoked when the WebContents is notified that the RenderView has been
  // fully created.
  virtual void RenderViewCreated(RenderViewHost* host) = 0;

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

  // If we try to close the tab while a drag is in progress, we crash.  These
  // methods allow the WebContents to determine if a drag is in progress and
  // postpone the tab closing.
  virtual bool IsDoingDrag() const = 0;
  virtual void CancelDragAndCloseTab() = 0;

  // If we close the tab while a UI control is in an event-tracking
  // loop, the control may message freed objects and crash.
  // WebContents::Close() calls IsEventTracking(), and if it returns
  // true CloseTabAfterEventTracking() is called and the close is not
  // completed.
  virtual bool IsEventTracking() const = 0;
  virtual void CloseTabAfterEventTracking() = 0;

  // Get the bounds of the View, relative to the parent.
  // TODO(beng): Return a rect rather than using an out param.
  virtual void GetViewBounds(gfx::Rect* out) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_H_
