// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_DEST_GTK_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_DEST_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "ui/base/gtk/gtk_signal.h"
#include "webkit/glue/webdropdata.h"

namespace content {

class RenderViewHostImpl;
class WebContents;
class WebDragDestDelegate;

// A helper class that handles DnD for drops in the renderer. In GTK parlance,
// this handles destination-side DnD, but not source-side DnD.
class CONTENT_EXPORT WebDragDestGtk {
 public:
  WebDragDestGtk(WebContents* web_contents, GtkWidget* widget);
  ~WebDragDestGtk();

  // This is called when the renderer responds to a drag motion event. We must
  // update the system drag cursor.
  void UpdateDragStatus(WebKit::WebDragOperation operation);

  // Informs the renderer when a system drag has left the render view.
  // See OnDragLeave().
  void DragLeave();

  WebDragDestDelegate* delegate() const { return delegate_; }
  void set_delegate(WebDragDestDelegate* delegate) { delegate_ = delegate; }

 private:
  content::RenderViewHostImpl* GetRenderViewHost() const;

  // Called when a system drag crosses over the render view. As there is no drag
  // enter event, we treat it as an enter event (and not a regular motion event)
  // when |context_| is NULL.
  CHROMEGTK_CALLBACK_4(WebDragDestGtk, gboolean, OnDragMotion, GdkDragContext*,
                       gint, gint, guint);

  // We make a series of requests for the drag data when the drag first enters
  // the render view. This is the callback that is used to give us the data
  // for each individual target. When |data_requests_| reaches 0, we know we
  // have attained all the data, and we can finally tell the renderer about the
  // drag.
  CHROMEGTK_CALLBACK_6(WebDragDestGtk, void, OnDragDataReceived,
                       GdkDragContext*, gint, gint, GtkSelectionData*,
                       guint, guint);

  // The drag has left our widget; forward this information to the renderer.
  CHROMEGTK_CALLBACK_2(WebDragDestGtk, void, OnDragLeave, GdkDragContext*,
                       guint);

  // Called by GTK when the user releases the mouse, executing a drop.
  CHROMEGTK_CALLBACK_4(WebDragDestGtk, gboolean, OnDragDrop, GdkDragContext*,
                       gint, gint, guint);

  WebContents* web_contents_;

  // The render view.
  GtkWidget* widget_;

  // The current drag context for system drags over our render view, or NULL if
  // there is no system drag or the system drag is not over our render view.
  GdkDragContext* context_;

  // The data for the current drag, or NULL if |context_| is NULL.
  scoped_ptr<WebDropData> drop_data_;

  // The number of outstanding drag data requests we have sent to the drag
  // source.
  int data_requests_;

  // The last time we sent a message to the renderer related to a drag motion.
  gint drag_over_time_;

  // Whether the cursor is over a drop target, according to the last message we
  // got from the renderer.
  bool is_drop_target_;

  // Handler ID for the destroy signal handler. We connect to the destroy
  // signal handler so that we won't call dest_unset on it after it is
  // destroyed, but we have to cancel the handler if we are destroyed before
  // |widget_| is.
  int destroy_handler_;

  // A delegate that can receive drag information about drag events.
  WebDragDestDelegate* delegate_;

  base::WeakPtrFactory<WebDragDestGtk> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebDragDestGtk);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_DEST_GTK_H_
