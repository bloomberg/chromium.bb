// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_drag_dest_gtk.h"

#include <string>

#include "app/gtk_dnd_util.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/gtk/bookmark_utils_gtk.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "net/base/net_util.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationLink;
using WebKit::WebDragOperationMove;

namespace {

WebDragOperation GdkDragActionToWebDragOp(GdkDragAction action) {
  WebDragOperation op = WebDragOperationNone;
  if (action & GDK_ACTION_COPY)
    op = static_cast<WebDragOperation>(op | WebDragOperationCopy);
  if (action & GDK_ACTION_LINK)
    op = static_cast<WebDragOperation>(op | WebDragOperationLink);
  if (action & GDK_ACTION_MOVE)
    op = static_cast<WebDragOperation>(op | WebDragOperationMove);
  return op;
}

}  // namespace

WebDragDestGtk::WebDragDestGtk(TabContents* tab_contents, GtkWidget* widget)
    : tab_contents_(tab_contents),
      widget_(widget),
      context_(NULL),
      method_factory_(this) {
  gtk_drag_dest_set(widget, static_cast<GtkDestDefaults>(0),
                    NULL, 0,
                    static_cast<GdkDragAction>(GDK_ACTION_COPY |
                                               GDK_ACTION_LINK |
                                               GDK_ACTION_MOVE));
  g_signal_connect(widget, "drag-motion",
                   G_CALLBACK(OnDragMotionThunk), this);
  g_signal_connect(widget, "drag-leave",
                   G_CALLBACK(OnDragLeaveThunk), this);
  g_signal_connect(widget, "drag-drop",
                   G_CALLBACK(OnDragDropThunk), this);
  g_signal_connect(widget, "drag-data-received",
                   G_CALLBACK(OnDragDataReceivedThunk), this);
  // TODO(tony): Need a drag-data-delete handler for moving content out of
  // the tab contents.  http://crbug.com/38989

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
    is_drop_target_ = operation != WebDragOperationNone;
    gdk_drag_status(context_, gtk_dnd_util::WebDragOpToGdkDragAction(operation),
                    drag_over_time_);
  }
}

void WebDragDestGtk::DragLeave() {
  tab_contents_->render_view_host()->DragTargetDragLeave();

  if (tab_contents_->GetBookmarkDragDelegate()) {
    tab_contents_->GetBookmarkDragDelegate()->OnDragLeave(bookmark_drag_data_);
  }
}

gboolean WebDragDestGtk::OnDragMotion(GtkWidget* sender,
                                      GdkDragContext* context,
                                      gint x, gint y,
                                      guint time) {
  if (context_ != context) {
    context_ = context;
    drop_data_.reset(new WebDropData);
    is_drop_target_ = false;

    static int supported_targets[] = {
      gtk_dnd_util::TEXT_PLAIN,
      gtk_dnd_util::TEXT_URI_LIST,
      gtk_dnd_util::TEXT_HTML,
      gtk_dnd_util::NETSCAPE_URL,
      gtk_dnd_util::CHROME_NAMED_URL,
      gtk_dnd_util::CHROME_BOOKMARK_ITEM,
      // TODO(estade): support image drags?
    };

    data_requests_ = arraysize(supported_targets);
    for (size_t i = 0; i < arraysize(supported_targets); ++i) {
      gtk_drag_get_data(widget_, context,
                        gtk_dnd_util::GetAtomForTarget(supported_targets[i]),
                        time);
    }
  } else if (data_requests_ == 0) {
    // TODO(snej): Pass appropriate DragOperation instead of hardcoding
    tab_contents_->render_view_host()->
        DragTargetDragOver(gtk_util::ClientPoint(widget_),
                           gtk_util::ScreenPoint(widget_),
                           GdkDragActionToWebDragOp(context->actions));
    if (tab_contents_->GetBookmarkDragDelegate())
      tab_contents_->GetBookmarkDragDelegate()->OnDragOver(bookmark_drag_data_);
    drag_over_time_ = time;
  }

  // Pretend we are a drag destination because we don't want to wait for
  // the renderer to tell us if we really are or not.
  return TRUE;
}

void WebDragDestGtk::OnDragDataReceived(
    GtkWidget* sender, GdkDragContext* context, gint x, gint y,
    GtkSelectionData* data, guint info, guint time) {
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
        gtk_dnd_util::GetAtomForTarget(gtk_dnd_util::TEXT_PLAIN)) {
      guchar* text = gtk_selection_data_get_text(data);
      if (text) {
        drop_data_->plain_text =
            UTF8ToUTF16(std::string(reinterpret_cast<char*>(text),
                                    data->length));
        g_free(text);
      }
    } else if (data->target ==
               gtk_dnd_util::GetAtomForTarget(gtk_dnd_util::TEXT_URI_LIST)) {
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
               gtk_dnd_util::GetAtomForTarget(gtk_dnd_util::TEXT_HTML)) {
      // TODO(estade): Can the html have a non-UTF8 encoding?
      drop_data_->text_html =
          UTF8ToUTF16(std::string(reinterpret_cast<char*>(data->data),
                                  data->length));
      // We leave the base URL empty.
    } else if (data->target ==
               gtk_dnd_util::GetAtomForTarget(gtk_dnd_util::NETSCAPE_URL)) {
      std::string netscape_url(reinterpret_cast<char*>(data->data),
                               data->length);
      size_t split = netscape_url.find_first_of('\n');
      if (split != std::string::npos) {
        drop_data_->url_title = UTF8ToUTF16(netscape_url.substr(0, split));
        if (split < netscape_url.size() - 1)
          drop_data_->url = GURL(netscape_url.substr(split + 1));
      }
    } else if (data->target ==
               gtk_dnd_util::GetAtomForTarget(gtk_dnd_util::CHROME_NAMED_URL)) {
      gtk_dnd_util::ExtractNamedURL(data,
                                    &drop_data_->url, &drop_data_->url_title);
    } else if (data->target ==
               gtk_dnd_util::GetAtomForTarget(
                   gtk_dnd_util::CHROME_BOOKMARK_ITEM)) {
      bookmark_drag_data_.ReadFromVector(
          bookmark_utils::GetNodesFromSelection(
              NULL, data,
              gtk_dnd_util::CHROME_BOOKMARK_ITEM,
              tab_contents_->profile(), NULL, NULL));
      bookmark_drag_data_.SetOriginatingProfile(tab_contents_->profile());
    }
  }

  if (data_requests_ == 0) {
    // Tell the renderer about the drag.
    // |x| and |y| are seemingly arbitrary at this point.
    // TODO(snej): Pass appropriate DragOperation instead of hardcoding.
    tab_contents_->render_view_host()->
        DragTargetDragEnter(*drop_data_.get(),
            gtk_util::ClientPoint(widget_),
            gtk_util::ScreenPoint(widget_),
            GdkDragActionToWebDragOp(context->actions));

    // This is non-null if tab_contents_ is showing an ExtensionDOMUI with
    // support for (at the moment experimental) drag and drop extensions.
    if (tab_contents_->GetBookmarkDragDelegate()) {
      tab_contents_->GetBookmarkDragDelegate()->OnDragEnter(
          bookmark_drag_data_);
    }

    drag_over_time_ = time;
  }
}

// The drag has left our widget; forward this information to the renderer.
void WebDragDestGtk::OnDragLeave(GtkWidget* sender, GdkDragContext* context,
                                 guint time) {
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
gboolean WebDragDestGtk::OnDragDrop(GtkWidget* sender, GdkDragContext* context,
                                    gint x, gint y, guint time) {
  // Cancel that drag leave!
  method_factory_.RevokeAll();

  tab_contents_->render_view_host()->
      DragTargetDrop(gtk_util::ClientPoint(widget_),
                     gtk_util::ScreenPoint(widget_));

  // This is non-null if tab_contents_ is showing an ExtensionDOMUI with
  // support for (at the moment experimental) drag and drop extensions.
  if (tab_contents_->GetBookmarkDragDelegate())
    tab_contents_->GetBookmarkDragDelegate()->OnDrop(bookmark_drag_data_);

  // The second parameter is just an educated guess as to whether or not the
  // drag succeeded, but at least we will get the drag-end animation right
  // sometimes.
  gtk_drag_finish(context, is_drop_target_, FALSE, time);
  return TRUE;
}
