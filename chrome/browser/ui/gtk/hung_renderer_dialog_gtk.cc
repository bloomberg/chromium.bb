// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hung_renderer_dialog.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/result_codes.h"
#include "gfx/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_signal.h"

namespace {

// A wrapper class that represents the Gtk dialog.
class HungRendererDialogGtk {
 public:
  HungRendererDialogGtk();
  void ShowForTabContents(TabContents* hung_contents);
  void EndForTabContents(TabContents* hung_contents);

 private:
  // The GtkTreeView column ids.
  enum {
    COL_FAVICON,
    COL_TITLE,
    COL_COUNT,
  };

  // Create the gtk dialog and add the widgets.
  void Init();

  CHROMEGTK_CALLBACK_1(HungRendererDialogGtk, void, OnDialogResponse, gint);

  GtkDialog* dialog_;
  GtkListStore* model_;
  TabContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(HungRendererDialogGtk);
};

// We only support showing one of these at a time per app.
HungRendererDialogGtk* g_instance = NULL;

// The response ID for the "Kill pages" button.  Anything positive should be
// fine (the built in GtkResponseTypes are negative numbers).
const int kKillPagesButtonResponse = 1;

HungRendererDialogGtk::HungRendererDialogGtk()
    : dialog_(NULL), model_(NULL), contents_(NULL) {
  Init();
}

void HungRendererDialogGtk::Init() {
  dialog_ = GTK_DIALOG(gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_BROWSER_HANGMONITOR_RENDERER_TITLE).c_str(),
      NULL,  // No parent because tabs can span multiple windows.
      GTK_DIALOG_NO_SEPARATOR,
      l10n_util::GetStringUTF8(IDS_BROWSER_HANGMONITOR_RENDERER_END).c_str(),
      kKillPagesButtonResponse,
      l10n_util::GetStringUTF8(IDS_BROWSER_HANGMONITOR_RENDERER_WAIT).c_str(),
      GTK_RESPONSE_OK,
      NULL));
  gtk_dialog_set_default_response(dialog_, GTK_RESPONSE_OK);
  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnDialogResponseThunk), this);

  // We have an hbox with the frozen icon on the left.  On the right,
  // we have a vbox with the unresponsive text on top and a table of
  // tabs on bottom.
  // ·-----------------------------------·
  // |·---------------------------------·|
  // ||·----·|·------------------------·||
  // |||icon|||                        |||
  // ||·----·|| The folowing page(s).. |||
  // ||      ||                        |||
  // ||      ||------------------------|||
  // ||      || table of tabs          |||
  // ||      |·------------------------·||
  // |·---------------------------------·|
  // |                                   |
  // |         kill button    wait button|
  // ·-----------------------------------·
  GtkWidget* contents_vbox = dialog_->vbox;
  gtk_box_set_spacing(GTK_BOX(contents_vbox), gtk_util::kContentAreaSpacing);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 12);
  gtk_box_pack_start(GTK_BOX(contents_vbox), hbox, TRUE, TRUE, 0);

  // Wrap the icon in a vbox so it stays top aligned.
  GtkWidget* icon_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), icon_vbox, FALSE, FALSE, 0);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* icon_pixbuf = rb.GetPixbufNamed(IDR_FROZEN_TAB_ICON);
  GtkWidget* icon = gtk_image_new_from_pixbuf(icon_pixbuf);
  gtk_box_pack_start(GTK_BOX(icon_vbox), icon, FALSE, FALSE, 0);

  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

  GtkWidget* text = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_BROWSER_HANGMONITOR_RENDERER).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(text), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), text, FALSE, FALSE, 0);

  GtkWidget* scroll_list = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_list),
      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_list),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(vbox), scroll_list, TRUE, TRUE, 0);

  // The list of hung tabs is GtkTreeView with a GtkListStore as the model.
  model_ = gtk_list_store_new(COL_COUNT, GDK_TYPE_PIXBUF, G_TYPE_STRING);
  GtkWidget* tree_view = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(model_));
  g_object_unref(model_);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);
  GtkTreeViewColumn* column = gtk_tree_view_column_new();
  GtkCellRenderer* renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(column, renderer, FALSE);
  gtk_tree_view_column_add_attribute(column, renderer, "pixbuf", COL_FAVICON);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, renderer, TRUE);
  gtk_tree_view_column_add_attribute(column, renderer, "text", COL_TITLE);

  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
  gtk_container_add(GTK_CONTAINER(scroll_list), tree_view);
}

void HungRendererDialogGtk::ShowForTabContents(TabContents* hung_contents) {
  DCHECK(hung_contents && dialog_);
  contents_ = hung_contents;
  gtk_list_store_clear(model_);

  GtkTreeIter tree_iter;
  for (TabContentsIterator it; !it.done(); ++it) {
    if (it->GetRenderProcessHost() == hung_contents->GetRenderProcessHost()) {
      gtk_list_store_append(model_, &tree_iter);
      std::string title = UTF16ToUTF8(it->GetTitle());
      if (title.empty())
        title = UTF16ToUTF8(TabContents::GetDefaultTitle());
      SkBitmap favicon = it->GetFavIcon();

      GdkPixbuf* pixbuf = NULL;
      if (favicon.width() > 0)
        pixbuf = gfx::GdkPixbufFromSkBitmap(&favicon);
      gtk_list_store_set(model_, &tree_iter,
          COL_FAVICON, pixbuf,
          COL_TITLE, title.c_str(),
          -1);
      if (pixbuf)
        g_object_unref(pixbuf);
    }
  }
  gtk_util::ShowDialog(GTK_WIDGET(dialog_));
}

void HungRendererDialogGtk::EndForTabContents(TabContents* contents) {
  DCHECK(contents);
  if (contents_ && contents_->GetRenderProcessHost() ==
      contents->GetRenderProcessHost()) {
    gtk_widget_hide(GTK_WIDGET(dialog_));
    // Since we're closing, we no longer need this TabContents.
    contents_ = NULL;
  }
}

// When the user clicks a button on the dialog or closes the dialog, this
// callback is called.
void HungRendererDialogGtk::OnDialogResponse(GtkWidget* widget,
                                             gint response_id) {
  DCHECK(g_instance == this);
  switch (response_id) {
    case kKillPagesButtonResponse:
      // Kill the process.
      if (contents_ && contents_->GetRenderProcessHost()) {
        base::KillProcess(contents_->GetRenderProcessHost()->GetHandle(),
                          ResultCodes::HUNG, false);
      }
      break;

    case GTK_RESPONSE_OK:
    case GTK_RESPONSE_DELETE_EVENT:
      // Start waiting again for responsiveness.
      if (contents_ && contents_->render_view_host())
        contents_->render_view_host()->RestartHangMonitorTimeout();
      break;
    default:
      NOTREACHED();
  }

  gtk_widget_destroy(GTK_WIDGET(dialog_));
  delete g_instance;
  g_instance = NULL;
}

}  // namespace

namespace hung_renderer_dialog {

void ShowForTabContents(TabContents* contents) {
  if (!logging::DialogsAreSuppressed()) {
    if (!g_instance)
      g_instance = new HungRendererDialogGtk();
    g_instance->ShowForTabContents(contents);
  }
}

// static
void HideForTabContents(TabContents* contents) {
  if (!logging::DialogsAreSuppressed() && g_instance)
    g_instance->EndForTabContents(contents);
}

}  // namespace hung_renderer_dialog
