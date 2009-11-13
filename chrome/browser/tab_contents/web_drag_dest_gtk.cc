// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_drag_dest_gtk.h"

#include "app/gtk_dnd_util.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "net/base/net_util.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationNone;

WebDragDestGtk::WebDragDestGtk(TabContents* tab_contents, GtkWidget* widget)
    : tab_contents_(tab_contents),
      widget_(widget),
      context_(NULL),
      method_factory_(this) {
  gtk_drag_dest_set(widget, static_cast<GtkDestDefaults>(0),
                    NULL, 0, GDK_ACTION_COPY);
  g_signal_connect(widget, "drag-motion",
                   G_CALLBACK(OnDragMotionThunk), this);
  g_signal_connect(widget, "drag-leave",
                   G_CALLBACK(OnDragLeaveThunk), this);
  g_signal_connect(widget, "drag-drop",
                   G_CALLBACK(OnDragDropThunk), this);
  g_signal_connect(widget, "drag-data-received",
                   G_CALLBACK(OnDragDataReceivedThunk), this);

  destroy_handler_ = g_signal_connect(
      widget, "destroy", G_CALLBACK(gtk_widget_destroyed), &widget_);
}

WebDragDestGtk::~WebDragDestGtk() {
  if (widget_) {
    gtk_drag_dest_unset(widget_);
    g_signal_handler_disconnect(widget_, destroy_handler_);
  }
}

void WebDragDestGtk::UpdateDragStatus(WebDragOperation operation) {
  if (context_) {
    // TODO(estade): we might want to support other actions besides copy,
    // but that would increase the cost of getting our drag success guess
    // wrong.
    is_drop_target_ = operation != WebDragOperationNone;
    // TODO(snej): Pass appropriate GDK action instead of hardcoding COPY
    gdk_drag_status(context_, is_drop_target_ ? GDK_ACTION_COPY :
                    static_cast<GdkDragAction>(0),
                    drag_over_time_);
  }
}

void WebDragDestGtk::DragLeave() {
  tab_contents_->render_view_host()->DragTargetDragLeave();
}

gboolean WebDragDestGtk::OnDragMotion(GdkDragContext* context, gint x, gint y,
                                      guint time) {
  if (context_ != context) {
    context_ = context;
    drop_data_.reset(new WebDropData);
    is_drop_target_ = false;

    static int supported_targets[] = {
      GtkDndUtil::TEXT_PLAIN,
      GtkDndUtil::TEXT_URI_LIST,
      GtkDndUtil::TEXT_HTML,
      // TODO(estade): support image drags?
    };

    data_requests_ = arraysize(supported_targets);
    for (size_t i = 0; i < arraysize(supported_targets); ++i) {
      gtk_drag_get_data(widget_, context,
                        GtkDndUtil::GetAtomForTarget(supported_targets[i]),
                        time);
    }
  } else if (data_requests_ == 0) {
    tab_contents_->render_view_host()->
        DragTargetDragOver(gtk_util::ClientPoint(widget_),
                           gtk_util::ScreenPoint(widget_),
                           WebDragOperationCopy);
    // TODO(snej): Pass appropriate DragOperation instead of hardcoding
    drag_over_time_ = time;
  }

  // Pretend we are a drag destination because we don't want to wait for
  // the renderer to tell us if we really are or not.
  return TRUE;
}

void WebDragDestGtk::OnDragDataReceived(
    GdkDragContext* context, gint x, gint y, GtkSelectionData* data,
    guint info, guint time) {
  // We might get the data from an old get_data() request that we no longer
  // care about.
  if (context != context_)
    return;

  data_requests_--;

  // Decode the data.
  if (data->data) {
    // If the source can't provide us with valid data for a requested target,
    // data->data will be NULL.
    if (data->target ==
        GtkDndUtil::GetAtomForTarget(GtkDndUtil::TEXT_PLAIN)) {
      guchar* text = gtk_selection_data_get_text(data);
      if (text) {
        drop_data_->plain_text =
            UTF8ToUTF16(std::string(reinterpret_cast<char*>(text),
                                    data->length));
        g_free(text);
      }
    } else if (data->target ==
               GtkDndUtil::GetAtomForTarget(GtkDndUtil::TEXT_URI_LIST)) {
      gchar** uris = gtk_selection_data_get_uris(data);
      if (uris) {
        for (gchar** uri_iter = uris; *uri_iter; uri_iter++) {
          // TODO(estade): Can the filenames have a non-UTF8 encoding?
          FilePath file_path;
          if (net::FileURLToFilePath(GURL(*uri_iter), &file_path))
            drop_data_->filenames.push_back(UTF8ToUTF16(file_path.value()));
        }
        // Also, write the first URI as the URL.
        if (uris[0])
          drop_data_->url = GURL(uris[0]);
        g_strfreev(uris);
      }
    } else if (data->target ==
               GtkDndUtil::GetAtomForTarget(GtkDndUtil::TEXT_HTML)) {
      // TODO(estade): Can the html have a non-UTF8 encoding?
      drop_data_->text_html =
          UTF8ToUTF16(std::string(reinterpret_cast<char*>(data->data),
                                  data->length));
      // We leave the base URL empty.
    }
  }

  if (data_requests_ == 0) {
    // Tell the renderer about the drag.
    // |x| and |y| are seemingly arbitrary at this point.
    tab_contents_->render_view_host()->
        DragTargetDragEnter(*drop_data_.get(),
                            gtk_util::ClientPoint(widget_),
                            gtk_util::ScreenPoint(widget_),
                            WebDragOperationCopy);
    // TODO(snej): Pass appropriate DragOperation instead of hardcoding
    drag_over_time_ = time;
  }
}

// The drag has left our widget; forward this information to the renderer.
void WebDragDestGtk::OnDragLeave(GdkDragContext* context, guint time) {
  // Set |context_| to NULL to make sure we will recognize the next DragMotion
  // as an enter.
  context_ = NULL;
  drop_data_.reset();
  // When GTK sends us a drag-drop signal, it is shortly (and synchronously)
  // preceded by a drag-leave. The renderer doesn't like getting the signals
  // in this order so delay telling it about the drag-leave till we are sure
  // we are not getting a drop as well.
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&WebDragDestGtk::DragLeave));
}

// Called by GTK when the user releases the mouse, executing a drop.
gboolean WebDragDestGtk::OnDragDrop(GdkDragContext* context, gint x, gint y,
                                    guint time) {
  // Cancel that drag leave!
  method_factory_.RevokeAll();

  tab_contents_->render_view_host()->
      DragTargetDrop(gtk_util::ClientPoint(widget_),
                     gtk_util::ScreenPoint(widget_));

  // The second parameter is just an educated guess, but at least we will
  // get the drag-end animation right sometimes.
  gtk_drag_finish(context, is_drop_target_, FALSE, time);
  return TRUE;
}
