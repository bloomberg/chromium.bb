// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_drag_dest_gtk.h"

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/drag_utils_gtk.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_drag_dest_delegate.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_util.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/base/gtk/gtk_screen_util.h"

using content::RenderViewHostImpl;
using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;

namespace content {

WebDragDestGtk::WebDragDestGtk(WebContents* web_contents, GtkWidget* widget)
    : web_contents_(web_contents),
      widget_(widget),
      context_(NULL),
      data_requests_(0),
      delegate_(NULL),
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
  // the WebContents.  http://crbug.com/38989

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
    gdk_drag_status(context_, content::WebDragOpToGdkDragAction(operation),
                    drag_over_time_);
  }
}

void WebDragDestGtk::DragLeave() {
  GetRenderViewHost()->DragTargetDragLeave();

  if (delegate())
    delegate()->OnDragLeave();

  drop_data_.reset();
}

gboolean WebDragDestGtk::OnDragMotion(GtkWidget* sender,
                                      GdkDragContext* context,
                                      gint x, gint y,
                                      guint time) {
  if (context_ != context) {
    context_ = context;
    drop_data_.reset(new WebDropData);
    is_drop_target_ = false;

    if (delegate())
      delegate()->DragInitialize(web_contents_);

    // text/plain must come before text/uri-list. This is a hack that works in
    // conjunction with OnDragDataReceived. Since some file managers populate
    // text/plain with file URLs when dragging files, we want to handle
    // text/uri-list after text/plain so that the plain text can be cleared if
    // it's a file drag.
    static int supported_targets[] = {
      ui::TEXT_PLAIN,
      ui::TEXT_URI_LIST,
      ui::TEXT_HTML,
      ui::NETSCAPE_URL,
      ui::CHROME_NAMED_URL,
      // TODO(estade): support image drags?
      ui::CUSTOM_DATA,
    };

    // Add the delegate's requested target if applicable. Need to do this here
    // since gtk_drag_get_data will dispatch to our drag-data-received.
    data_requests_ = arraysize(supported_targets) + (delegate() ? 1 : 0);
    for (size_t i = 0; i < arraysize(supported_targets); ++i) {
      gtk_drag_get_data(widget_, context,
                        ui::GetAtomForTarget(supported_targets[i]),
                        time);
    }

    if (delegate()) {
      gtk_drag_get_data(widget_, context, delegate()->GetBookmarkTargetAtom(),
                        time);
    }
  } else if (data_requests_ == 0) {
    GetRenderViewHost()->DragTargetDragOver(
        ui::ClientPoint(widget_),
        ui::ScreenPoint(widget_),
        content::GdkDragActionToWebDragOp(context->actions));

    if (delegate())
      delegate()->OnDragOver();

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
  gint data_length = gtk_selection_data_get_length(data);
  const guchar* raw_data = gtk_selection_data_get_data(data);
  GdkAtom target = gtk_selection_data_get_target(data);
  if (raw_data && data_length > 0) {
    // If the source can't provide us with valid data for a requested target,
    // raw_data will be NULL.
    if (target == ui::GetAtomForTarget(ui::TEXT_PLAIN)) {
      guchar* text = gtk_selection_data_get_text(data);
      if (text) {
        drop_data_->plain_text =
            UTF8ToUTF16(std::string(reinterpret_cast<const char*>(text)));
        g_free(text);
      }
    } else if (target == ui::GetAtomForTarget(ui::TEXT_URI_LIST)) {
      gchar** uris = gtk_selection_data_get_uris(data);
      if (uris) {
        drop_data_->url = GURL();
        for (gchar** uri_iter = uris; *uri_iter; uri_iter++) {
          // Most file managers populate text/uri-list with file URLs when
          // dragging files. To avoid exposing file system paths to web content,
          // file URLs are never set as the URL content for the drop.
          // TODO(estade): Can the filenames have a non-UTF8 encoding?
          GURL url(*uri_iter);
          FilePath file_path;
          if (url.SchemeIs(chrome::kFileScheme) &&
              net::FileURLToFilePath(url, &file_path)) {
            drop_data_->filenames.push_back(UTF8ToUTF16(file_path.value()));
            // This is a hack. Some file managers also populate text/plain with
            // a file URL when dragging files, so we clear it to avoid exposing
            // it to the web content.
            drop_data_->plain_text.clear();
          } else if (!drop_data_->url.is_valid()) {
            // Also set the first non-file URL as the URL content for the drop.
            drop_data_->url = url;
          }
        }
        g_strfreev(uris);
      }
    } else if (target == ui::GetAtomForTarget(ui::TEXT_HTML)) {
      // TODO(estade): Can the html have a non-UTF8 encoding?
      drop_data_->text_html =
          UTF8ToUTF16(std::string(reinterpret_cast<const char*>(raw_data),
                                  data_length));
      // We leave the base URL empty.
    } else if (target == ui::GetAtomForTarget(ui::NETSCAPE_URL)) {
      std::string netscape_url(reinterpret_cast<const char*>(raw_data),
                               data_length);
      size_t split = netscape_url.find_first_of('\n');
      if (split != std::string::npos) {
        drop_data_->url = GURL(netscape_url.substr(0, split));
        if (split < netscape_url.size() - 1)
          drop_data_->url_title = UTF8ToUTF16(netscape_url.substr(split + 1));
      }
    } else if (target == ui::GetAtomForTarget(ui::CHROME_NAMED_URL)) {
      ui::ExtractNamedURL(data, &drop_data_->url, &drop_data_->url_title);
    } else if (target == ui::GetAtomForTarget(ui::CUSTOM_DATA)) {
      ui::ReadCustomDataIntoMap(
          raw_data, data_length, &drop_data_->custom_data);
    }
  }

  // For CHROME_BOOKMARK_ITEM, we have to handle the case where the drag source
  // doesn't have any data available for us. In this case we try to synthesize a
  // URL bookmark.
  // Note that bookmark drag data is encoded in the same format for both
  // GTK and Views, hence we can share the same logic here.
  if (delegate() && target == delegate()->GetBookmarkTargetAtom()) {
    if (raw_data && data_length > 0) {
      delegate()->OnReceiveDataFromGtk(data);
    } else {
      delegate()->OnReceiveProcessedData(drop_data_->url,
                                         drop_data_->url_title);
    }
  }

  if (data_requests_ == 0) {
    // Tell the renderer about the drag.
    // |x| and |y| are seemingly arbitrary at this point.
    GetRenderViewHost()->DragTargetDragEnter(
        *drop_data_.get(),
        ui::ClientPoint(widget_),
        ui::ScreenPoint(widget_),
        content::GdkDragActionToWebDragOp(context->actions));

    if (delegate())
      delegate()->OnDragEnter();

    drag_over_time_ = time;
  }
}

// The drag has left our widget; forward this information to the renderer.
void WebDragDestGtk::OnDragLeave(GtkWidget* sender, GdkDragContext* context,
                                 guint time) {
  // Set |context_| to NULL to make sure we will recognize the next DragMotion
  // as an enter.
  context_ = NULL;

  // When GTK sends us a drag-drop signal, it is shortly (and synchronously)
  // preceded by a drag-leave. The renderer doesn't like getting the signals
  // in this order so delay telling it about the drag-leave till we are sure
  // we are not getting a drop as well.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&WebDragDestGtk::DragLeave, method_factory_.GetWeakPtr()));
}

// Called by GTK when the user releases the mouse, executing a drop.
gboolean WebDragDestGtk::OnDragDrop(GtkWidget* sender, GdkDragContext* context,
                                    gint x, gint y, guint time) {
  // Cancel that drag leave!
  method_factory_.InvalidateWeakPtrs();

  GetRenderViewHost()->
      DragTargetDrop(ui::ClientPoint(widget_), ui::ScreenPoint(widget_));

  if (delegate())
    delegate()->OnDrop();

  // The second parameter is just an educated guess as to whether or not the
  // drag succeeded, but at least we will get the drag-end animation right
  // sometimes.
  gtk_drag_finish(context, is_drop_target_, FALSE, time);

  return TRUE;
}

RenderViewHostImpl* WebDragDestGtk::GetRenderViewHost() const {
  return static_cast<RenderViewHostImpl*>(web_contents_->GetRenderViewHost());
}

}  // namespace content
