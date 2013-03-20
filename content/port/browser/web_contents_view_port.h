// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PORT_BROWSER_WEB_CONTENTS_VIEW_PORT_H_
#define CONTENT_PORT_BROWSER_WEB_CONTENTS_VIEW_PORT_H_

#include "content/public/browser/web_contents_view.h"

namespace content {
class RenderViewHost;
class RenderWidgetHost;
class RenderWidgetHostView;

// This is the larger WebContentsView interface exposed only within content/ and
// to embedders looking to port to new platforms.
class CONTENT_EXPORT WebContentsViewPort : public WebContentsView {
 public:
  virtual ~WebContentsViewPort() {}

  virtual void CreateView(
      const gfx::Size& initial_size, gfx::NativeView context) = 0;

  // Sets up the View that holds the rendered web page, receives messages for
  // it and contains page plugins. The host view should be sized to the current
  // size of the WebContents.
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) = 0;

  // Creates a new View that holds a popup and receives messages for it.
  virtual RenderWidgetHostView* CreateViewForPopupWidget(
      RenderWidgetHost* render_widget_host) = 0;

  // Sets the page title for the native widgets corresponding to the view. This
  // is not strictly necessary and isn't expected to be displayed anywhere, but
  // can aid certain debugging tools such as Spy++ on Windows where you are
  // trying to find a specific window.
  virtual void SetPageTitle(const string16& title) = 0;

  // Invoked when the WebContents is notified that the RenderView has been
  // fully created.
  virtual void RenderViewCreated(RenderViewHost* host) = 0;

  // Invoked when the WebContents is notified that the RenderView has been
  // swapped in.
  virtual void RenderViewSwappedIn(RenderViewHost* host) = 0;

  // Invoked to enable/disable overscroll gesture navigation.
  virtual void SetOverscrollControllerEnabled(bool enabled) = 0;

#if defined(OS_MACOSX)
  // If we close the tab while a UI control is in an event-tracking
  // loop, the control may message freed objects and crash.
  // WebContents::Close() calls IsEventTracking(), and if it returns
  // true CloseTabAfterEventTracking() is called and the close is not
  // completed.
  virtual bool IsEventTracking() const = 0;
  virtual void CloseTabAfterEventTracking() = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PORT_BROWSER_WEB_CONTENTS_VIEW_PORT_H_
