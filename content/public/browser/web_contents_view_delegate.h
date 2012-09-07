// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_WIN_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_WIN_DELEGATE_H_

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#elif defined(OS_MACOSX)
#import <Cocoa/Cocoa.h>
#endif

#include "content/common/content_export.h"
#include "content/public/common/context_menu_source_type.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_MACOSX)
@protocol RenderWidgetHostViewMacDelegate;
#endif

namespace gfx {
class Size;
}

namespace ui {
class FocusStoreGtk;
}

namespace content {

class RenderWidgetHost;
class WebDragDestDelegate;
struct ContextMenuParams;

// This interface allows a client to extend the functionality of the
// WebContentsView implementation.
class CONTENT_EXPORT WebContentsViewDelegate {
 public:
  virtual ~WebContentsViewDelegate() {}

  // Returns a delegate to process drags not handled by content.
  virtual WebDragDestDelegate* GetDragDestDelegate() = 0;

  // Shows a context menu.
  virtual void ShowContextMenu(
      const content::ContextMenuParams& params,
      const content::ContextMenuSourceType& type) = 0;

#if defined(OS_WIN) || defined(USE_AURA)
  // These methods allow the embedder to intercept WebContentsViewWin's
  // implementation of these WebContentsView methods. See the WebContentsView
  // interface documentation for more information about these methods.
  virtual void StoreFocus() = 0;
  virtual void RestoreFocus() = 0;
  virtual bool Focus() = 0;
  virtual void TakeFocus(bool reverse) = 0;
  virtual void SizeChanged(const gfx::Size& size) = 0;
#elif defined(TOOLKIT_GTK)
  // Initializes the WebContentsViewDelegate.
  virtual void Initialize(GtkWidget* expanded_container,
                          ui::FocusStoreGtk* focus_store) = 0;

  // Returns the top widget that contains |view| passed in from WrapView. This
  // is exposed through WebContentsViewGtk::GetNativeView() when a wrapper is
  // supplied to a WebContentsViewGtk.
  virtual gfx::NativeView GetNativeView() const = 0;

  // Handles a focus event from the renderer process.
  virtual void Focus() = 0;

  // Gives the delegate a first chance at focus events from our render widget
  // host, before the main view invokes its default behaviour. Returns TRUE if
  // |return_value| has been set and that value should be returned to GTK+.
  virtual gboolean OnNativeViewFocusEvent(GtkWidget* widget,
                                          GtkDirectionType type,
                                          gboolean* return_value) = 0;
#elif defined(OS_MACOSX)
  // Returns a newly-created delegate for the RenderWidgetHostViewMac, to handle
  // events on the responder chain.
  virtual NSObject<RenderWidgetHostViewMacDelegate>*
      CreateRenderWidgetHostViewDelegate(
          RenderWidgetHost* render_widget_host) = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_WIN_DELEGATE_H_
