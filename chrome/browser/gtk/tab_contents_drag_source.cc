// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tab_contents_drag_source.h"

#include "app/gtk_dnd_util.h"
#include "base/mime_util.h"
#include "base/string_util.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/gtk_util.h"
#include "webkit/glue/webdropdata.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;

TabContentsDragSource::TabContentsDragSource(
    TabContentsView* tab_contents_view)
    : tab_contents_view_(tab_contents_view),
      drag_failed_(false),
      drag_widget_(NULL) {
  drag_widget_ = gtk_invisible_new();
  g_signal_connect(drag_widget_, "drag-failed",
                   G_CALLBACK(OnDragFailedThunk), this);
  g_signal_connect(drag_widget_, "drag-end", G_CALLBACK(OnDragEndThunk), this);
  g_signal_connect(drag_widget_, "drag-data-get",
                   G_CALLBACK(OnDragDataGetThunk), this);
  g_object_ref_sink(drag_widget_);
}

TabContentsDragSource::~TabContentsDragSource() {
  g_signal_handlers_disconnect_by_func(drag_widget_,
      reinterpret_cast<gpointer>(OnDragFailedThunk), this);
  g_signal_handlers_disconnect_by_func(drag_widget_,
      reinterpret_cast<gpointer>(OnDragEndThunk), this);
  g_signal_handlers_disconnect_by_func(drag_widget_,
      reinterpret_cast<gpointer>(OnDragDataGetThunk), this);

  // Break the current drag, if any.
  if (drop_data_.get()) {
    gtk_grab_add(drag_widget_);
    gtk_grab_remove(drag_widget_);
    MessageLoopForUI::current()->RemoveObserver(this);
    drop_data_.reset();
  }

  gtk_widget_destroy(drag_widget_);
  g_object_unref(drag_widget_);
  drag_widget_ = NULL;
}

TabContents* TabContentsDragSource::tab_contents() const {
  return tab_contents_view_->tab_contents();
}

void TabContentsDragSource::StartDragging(const WebDropData& drop_data,
                                          GdkEventButton* last_mouse_down) {
  int targets_mask = 0;

  if (!drop_data.plain_text.empty())
    targets_mask |= GtkDndUtil::TEXT_PLAIN;
  if (drop_data.url.is_valid()) {
    targets_mask |= GtkDndUtil::TEXT_URI_LIST;
    targets_mask |= GtkDndUtil::CHROME_NAMED_URL;
    targets_mask |= GtkDndUtil::NETSCAPE_URL;
  }
  if (!drop_data.text_html.empty())
    targets_mask |= GtkDndUtil::TEXT_HTML;
  if (!drop_data.file_contents.empty())
    targets_mask |= GtkDndUtil::CHROME_WEBDROP_FILE_CONTENTS;

  if (targets_mask == 0) {
    NOTIMPLEMENTED();
    if (tab_contents()->render_view_host())
      tab_contents()->render_view_host()->DragSourceSystemDragEnded();
  }

  drop_data_.reset(new WebDropData(drop_data));

  GtkTargetList* list = GtkDndUtil::GetTargetListFromCodeMask(targets_mask);
  if (targets_mask & GtkDndUtil::CHROME_WEBDROP_FILE_CONTENTS) {
    drag_file_mime_type_ = gdk_atom_intern(
        mime_util::GetDataMimeType(drop_data.file_contents).c_str(), FALSE);
    gtk_target_list_add(list, drag_file_mime_type_,
                        0, GtkDndUtil::CHROME_WEBDROP_FILE_CONTENTS);
  }

  drag_failed_ = false;
  // If we don't pass an event, GDK won't know what event time to start grabbing
  // mouse events. Technically it's the mouse motion event and not the mouse
  // down event that causes the drag, but there's no reliable way to know
  // *which* motion event initiated the drag, so this will have to do.
  // TODO(estade): This can sometimes be very far off, e.g. if the user clicks
  // and holds and doesn't start dragging for a long time. I doubt it matters
  // much, but we should probably look into the possibility of getting the
  // initiating event from webkit.
  gtk_drag_begin(drag_widget_, list,
                 static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_LINK),
                 1,  // Drags are always initiated by the left button.
                 reinterpret_cast<GdkEvent*>(last_mouse_down));
  MessageLoopForUI::current()->AddObserver(this);
  // The drag adds a ref; let it own the list.
  gtk_target_list_unref(list);
}

void TabContentsDragSource::WillProcessEvent(GdkEvent* event) {
  // No-op.
}

void TabContentsDragSource::DidProcessEvent(GdkEvent* event) {
  if (event->type != GDK_MOTION_NOTIFY)
    return;

  GdkEventMotion* event_motion = reinterpret_cast<GdkEventMotion*>(event);
  gfx::Point client = gtk_util::ClientPoint(GetContentNativeView());

  if (tab_contents()->render_view_host()) {
    tab_contents()->render_view_host()->DragSourceMovedTo(
        client.x(), client.y(),
        static_cast<int>(event_motion->x_root),
        static_cast<int>(event_motion->y_root));
  }
}

void TabContentsDragSource::OnDragDataGet(
    GdkDragContext* context, GtkSelectionData* selection_data,
    guint target_type, guint time) {
  const int bits_per_byte = 8;

  switch (target_type) {
    case GtkDndUtil::TEXT_PLAIN: {
      std::string utf8_text = UTF16ToUTF8(drop_data_->plain_text);
      gtk_selection_data_set_text(selection_data, utf8_text.c_str(),
                                  utf8_text.length());
      break;
    }

    case GtkDndUtil::TEXT_HTML: {
      // TODO(estade): change relative links to be absolute using
      // |html_base_url|.
      std::string utf8_text = UTF16ToUTF8(drop_data_->text_html);
      gtk_selection_data_set(selection_data,
                             GtkDndUtil::GetAtomForTarget(
                                 GtkDndUtil::TEXT_HTML),
                             bits_per_byte,
                             reinterpret_cast<const guchar*>(utf8_text.c_str()),
                             utf8_text.length());
      break;
    }

    case GtkDndUtil::TEXT_URI_LIST:
    case GtkDndUtil::CHROME_NAMED_URL: {
      GtkDndUtil::WriteURLWithName(selection_data, drop_data_->url,
                                   drop_data_->url_title, target_type);
      break;
    }

    case GtkDndUtil::NETSCAPE_URL: {
      // _NETSCAPE_URL format is URL + \n + title.
      std::string utf8_text = drop_data_->url.spec() + "\n" + UTF16ToUTF8(
          drop_data_->url_title);
      gtk_selection_data_set(selection_data,
                             selection_data->target,
                             bits_per_byte,
                             reinterpret_cast<const guchar*>(utf8_text.c_str()),
                             utf8_text.length());
      break;
    }

    case GtkDndUtil::CHROME_WEBDROP_FILE_CONTENTS: {
      gtk_selection_data_set(
          selection_data,
          drag_file_mime_type_, bits_per_byte,
          reinterpret_cast<const guchar*>(drop_data_->file_contents.data()),
          drop_data_->file_contents.length());
      break;
    }

    default:
      NOTREACHED();
  }
}

gboolean TabContentsDragSource::OnDragFailed() {
  drag_failed_ = true;

  gfx::Point root = gtk_util::ScreenPoint(GetContentNativeView());
  gfx::Point client = gtk_util::ClientPoint(GetContentNativeView());

  if (tab_contents()->render_view_host()) {
    tab_contents()->render_view_host()->DragSourceEndedAt(
        client.x(), client.y(), root.x(), root.y(),
        WebDragOperationNone);
  }

  // Let the native failure animation run.
  return FALSE;
}

void TabContentsDragSource::OnDragEnd(WebDragOperation operation) {
  MessageLoopForUI::current()->RemoveObserver(this);

  if (!drag_failed_) {
    gfx::Point root = gtk_util::ScreenPoint(GetContentNativeView());
    gfx::Point client = gtk_util::ClientPoint(GetContentNativeView());

    if (tab_contents()->render_view_host()) {
      tab_contents()->render_view_host()->DragSourceEndedAt(
          client.x(), client.y(), root.x(), root.y(), operation);
    }
  }

  if (tab_contents()->render_view_host())
    tab_contents()->render_view_host()->DragSourceSystemDragEnded();

  drop_data_.reset();
}

gfx::NativeView TabContentsDragSource::GetContentNativeView() const {
  return tab_contents_view_->GetContentNativeView();
}
