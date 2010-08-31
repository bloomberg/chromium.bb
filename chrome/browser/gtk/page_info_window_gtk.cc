// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "build/build_config.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/compiler_specific.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/page_info_model.h"
#include "chrome/browser/page_info_window.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

namespace {

enum {
  RESPONSE_SHOW_CERT_INFO = 0,
};

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

  // Shows the certificate info window.
  void ShowCertDialog();

  GtkWidget* widget() { return dialog_; }

 private:
  // Layouts the different sections retrieved from the model.
  void InitContents();

  // Returns a widget that contains the UI for the passed |section|.
  GtkWidget* CreateSection(const PageInfoModel::SectionInfo& section);

  // The model containing the different sections to display.
  PageInfoModel model_;

  // The page info dialog.
  GtkWidget* dialog_;

  // The url for this dialog. Should be unique among active dialogs.
  GURL url_;

  // The virtual box containing the sections.
  GtkWidget* contents_;

  // The id of the certificate for this page.
  int cert_id_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoWindowGtk);
};

// We only show one page info per URL (keyed on url.spec()).
typedef std::map<std::string, PageInfoWindowGtk*> PageInfoWindowMap;
PageInfoWindowMap g_page_info_window_map;

// Button callbacks.
void OnDialogResponse(GtkDialog* dialog, gint response_id,
                      PageInfoWindowGtk* page_info) {
  if (response_id == RESPONSE_SHOW_CERT_INFO) {
    page_info->ShowCertDialog();
  } else {
    // "Close" was clicked.
    gtk_widget_destroy(GTK_WIDGET(dialog));
  }
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
      url_(url),
      contents_(NULL),
      cert_id_(ssl.cert_id()) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_PAGEINFO_WINDOW_TITLE).c_str(),
      parent,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      NULL);
  if (cert_id_) {
    gtk_dialog_add_button(
        GTK_DIALOG(dialog_),
        l10n_util::GetStringUTF8(IDS_PAGEINFO_CERT_INFO_BUTTON).c_str(),
        RESPONSE_SHOW_CERT_INFO);
  }
  gtk_dialog_add_button(GTK_DIALOG(dialog_), GTK_STOCK_CLOSE,
                        GTK_RESPONSE_CLOSE);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_CLOSE);

  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);
  g_signal_connect(dialog_, "response", G_CALLBACK(OnDialogResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroy), this);

  InitContents();

  g_page_info_window_map[url.spec()] = this;
}

PageInfoWindowGtk::~PageInfoWindowGtk() {
  g_page_info_window_map.erase(url_.spec());
}

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
  GtkWidget* image = gtk_image_new_from_pixbuf(
      section.state != PageInfoModel::SECTION_STATE_ERROR ?
      rb.GetPixbufNamed(IDR_PAGEINFO_GOOD) :
      rb.GetPixbufNamed(IDR_PAGEINFO_BAD));
  gtk_box_pack_start(GTK_BOX(section_box), image, FALSE, FALSE,
                     gtk_util::kControlSpacing);
  gtk_misc_set_alignment(GTK_MISC(image), 0, 0);

  GtkWidget* text_box = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  if (!section.headline.empty()) {
    label = gtk_label_new(UTF16ToUTF8(section.headline).c_str());
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_box_pack_start(GTK_BOX(text_box), label, FALSE, FALSE, 0);
  }
  label = gtk_label_new(UTF16ToUTF8(section.description).c_str());
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  // Allow linebreaking in the middle of words if necessary, so that extremely
  // long hostnames (longer than one line) will still be completely shown.
  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
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

void PageInfoWindowGtk::ShowCertDialog() {
  ShowCertificateViewerByID(GTK_WINDOW(dialog_), cert_id_);
}

}  // namespace

namespace browser {

void ShowPageInfo(gfx::NativeWindow parent,
                  Profile* profile,
                  const GURL& url,
                  const NavigationEntry::SSLStatus& ssl,
                  bool show_history) {
  PageInfoWindowMap::iterator iter =
      g_page_info_window_map.find(url.spec());
  if (iter != g_page_info_window_map.end()) {
    gtk_window_present(GTK_WINDOW(iter->second->widget()));
    return;
  }

  PageInfoWindowGtk* page_info_window =
      new PageInfoWindowGtk(parent, profile, url, ssl, show_history);
  page_info_window->Show();
}

void ShowPageInfoBubble(gfx::NativeWindow parent,
                        Profile* profile,
                        const GURL& url,
                        const NavigationEntry::SSLStatus& ssl,
                        bool show_history) {
  NOTIMPLEMENTED();
}

}  // namespace browser
