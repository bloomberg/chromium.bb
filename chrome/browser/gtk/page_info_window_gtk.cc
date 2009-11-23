// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "build/build_config.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "chrome/browser/page_info_model.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/common/gtk_util.h"
#include "grit/locale_settings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

class PageInfoWindowGtk : public PageInfoModel::PageInfoModelObserver {
 public:
  PageInfoWindowGtk(gfx::NativeWindow parent,
                    Profile* profile,
                    const GURL& url,
                    const NavigationEntry::SSLStatus& ssl,
                    bool show_history);
  ~PageInfoWindowGtk();

  // PageInfoModelObserver implementation:
  virtual void ModelChanged();

  // Shows the page info window.
  void Show();

 private:
  // Layouts the different sections retrieved from the model.
  void InitContents();

  // Returns a widget that contains the UI for the passed |section|.
  GtkWidget* CreateSection(const PageInfoModel::SectionInfo& section);

  // The model containing the different sections to display.
  PageInfoModel model_;

  // The page info dialog.
  GtkWidget* dialog_;

  // The virtual box containing the sections.
  GtkWidget* contents_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoWindowGtk);
};

// Close button callback.
void OnDialogResponse(GtkDialog* dialog, gpointer data) {
  // "Close" was clicked.
  gtk_widget_destroy(GTK_WIDGET(dialog));
}

void OnDestroy(GtkDialog* dialog, PageInfoWindowGtk* page_info) {
  delete page_info;
}

////////////////////////////////////////////////////////////////////////////////
// PageInfoWindowGtk, public:
PageInfoWindowGtk::PageInfoWindowGtk(gfx::NativeWindow parent,
                                     Profile* profile,
                                     const GURL& url,
                                     const NavigationEntry::SSLStatus& ssl,
                                     bool show_history)
    : ALLOW_THIS_IN_INITIALIZER_LIST(model_(profile, url, ssl,
                                            show_history, this)),
      contents_(NULL) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_PAGEINFO_WINDOW_TITLE).c_str(),
      parent,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);
  g_signal_connect(dialog_, "response", G_CALLBACK(OnDialogResponse), NULL);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroy), this);

  InitContents();
}

PageInfoWindowGtk::~PageInfoWindowGtk() {}

void PageInfoWindowGtk::ModelChanged() {
  InitContents();
}

GtkWidget* PageInfoWindowGtk::CreateSection(
    const PageInfoModel::SectionInfo& section) {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  GtkWidget* label = gtk_label_new(UTF16ToUTF8(section.title).c_str());

  PangoAttrList* attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes,
                         pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes(GTK_LABEL(label), attributes);
  pango_attr_list_unref(attributes);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  GtkWidget* section_box = gtk_hbox_new(FALSE, 0);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GtkWidget* image = gtk_image_new_from_pixbuf(section.state ?
      rb.GetPixbufNamed(IDR_PAGEINFO_GOOD) :
      rb.GetPixbufNamed(IDR_PAGEINFO_BAD));
  gtk_box_pack_start(GTK_BOX(section_box), image, FALSE, FALSE,
                     gtk_util::kControlSpacing);
  gtk_misc_set_alignment(GTK_MISC(image), 0, 0);

  GtkWidget* text_box = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  if (!section.head_line.empty()) {
    label = gtk_label_new(UTF16ToUTF8(section.head_line).c_str());
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_box_pack_start(GTK_BOX(text_box), label, FALSE, FALSE, 0);
  }
  label = gtk_label_new(UTF16ToUTF8(section.description).c_str());
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_box_pack_start(GTK_BOX(text_box), label, FALSE, FALSE, 0);
  gtk_widget_set_size_request(label, 400, -1);

  gtk_box_pack_start(GTK_BOX(section_box), text_box, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), section_box, TRUE, TRUE, 0);

  return vbox;
}

void PageInfoWindowGtk::InitContents() {
  if (contents_)
    gtk_widget_destroy(contents_);
  contents_ = gtk_vbox_new(FALSE, gtk_util::kContentAreaSpacing);
  for (int i = 0; i < model_.GetSectionCount(); i++) {
    gtk_box_pack_start(GTK_BOX(contents_),
                       CreateSection(model_.GetSectionInfo(i)),
                       FALSE, FALSE, 0);
  }
  gtk_widget_show_all(contents_);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), contents_);
}

void PageInfoWindowGtk::Show() {
  gtk_widget_show(dialog_);
}

} // namespace

namespace browser {

void ShowPageInfo(gfx::NativeWindow parent,
                  Profile* profile,
                  const GURL& url,
                  const NavigationEntry::SSLStatus& ssl,
                  bool show_history) {
  PageInfoWindowGtk* window = new PageInfoWindowGtk(parent, profile, url,
                                                    ssl, show_history);
  window->Show();
}

}  // namespace browser
