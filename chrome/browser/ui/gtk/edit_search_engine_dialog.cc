// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/edit_search_engine_dialog.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/ui/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "googleurl/src/gurl.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

std::string GetDisplayURL(const TemplateURL& turl) {
  return turl.url() ? UTF16ToUTF8(turl.url()->DisplayURL()) : std::string();
}

// Forces text to lowercase when connected to an editable's "insert-text"
// signal.  (Like views Textfield::STYLE_LOWERCASE.)
void LowercaseInsertTextHandler(GtkEditable *editable, const gchar *text,
                                gint length, gint *position, gpointer data) {
  string16 original_text = UTF8ToUTF16(text);
  string16 lower_text = l10n_util::ToLower(original_text);
  if (lower_text != original_text) {
    std::string result = UTF16ToUTF8(lower_text);
    // Prevent ourselves getting called recursively about our own edit.
    g_signal_handlers_block_by_func(G_OBJECT(editable),
        reinterpret_cast<gpointer>(LowercaseInsertTextHandler), data);
    gtk_editable_insert_text(editable, result.c_str(), result.size(), position);
    g_signal_handlers_unblock_by_func(G_OBJECT(editable),
        reinterpret_cast<gpointer>(LowercaseInsertTextHandler), data);
    // We've inserted our modified version, stop the defalut handler from
    // inserting the original.
    g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
  }
}

void SetWidgetStyle(GtkWidget* entry, GtkStyle* label_style,
                    GtkStyle* dialog_style) {
  gtk_widget_modify_fg(entry, GTK_STATE_NORMAL,
                       &label_style->fg[GTK_STATE_NORMAL]);
  gtk_widget_modify_fg(entry, GTK_STATE_INSENSITIVE,
                       &label_style->fg[GTK_STATE_INSENSITIVE]);
  // GTK_NO_WINDOW widgets like GtkLabel don't draw their own background, so we
  // combine the normal or insensitive foreground of the label style with the
  // normal background of the window style to achieve the "normal label" and
  // "insensitive label" colors.
  gtk_widget_modify_base(entry, GTK_STATE_NORMAL,
                         &dialog_style->bg[GTK_STATE_NORMAL]);
  gtk_widget_modify_base(entry, GTK_STATE_INSENSITIVE,
                         &dialog_style->bg[GTK_STATE_NORMAL]);
}

}  // namespace

EditSearchEngineDialog::EditSearchEngineDialog(
    GtkWindow* parent_window,
    const TemplateURL* template_url,
    EditSearchEngineControllerDelegate* delegate,
    Profile* profile)
    : controller_(new EditSearchEngineController(template_url, delegate,
                                                 profile)) {
  Init(parent_window, profile);
}

EditSearchEngineDialog::~EditSearchEngineDialog() {}

void EditSearchEngineDialog::Init(GtkWindow* parent_window, Profile* profile) {
  std::string dialog_name = l10n_util::GetStringUTF8(
      controller_->template_url() ?
      IDS_SEARCH_ENGINES_EDITOR_EDIT_WINDOW_TITLE :
      IDS_SEARCH_ENGINES_EDITOR_NEW_WINDOW_TITLE);

  dialog_ = gtk_dialog_new_with_buttons(
      dialog_name.c_str(),
      parent_window,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      NULL);

  accessible_widget_helper_.reset(new AccessibleWidgetHelper(
      dialog_, profile));
  accessible_widget_helper_->SendOpenWindowNotification(dialog_name);

  ok_button_ = gtk_dialog_add_button(GTK_DIALOG(dialog_),
                                     controller_->template_url() ?
                                     GTK_STOCK_SAVE :
                                     GTK_STOCK_ADD,
                                     GTK_RESPONSE_OK);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_OK);

  // The dialog layout hierarchy looks like this:
  //
  // \ GtkVBox |dialog_->vbox|
  // +-\ GtkTable |controls|
  // | +-\ row 0
  // | | +- GtkLabel
  // | | +-\ GtkHBox
  // | |   +- GtkEntry |title_entry_|
  // | |   +- GtkImage |title_image_|
  // | +-\ row 1
  // | | +- GtkLabel
  // | | +-\ GtkHBox
  // | |   +- GtkEntry |keyword_entry_|
  // | |   +- GtkImage |keyword_image_|
  // | +-\ row 2
  // |   +- GtkLabel
  // |   +-\ GtkHBox
  // |     +- GtkEntry |url_entry_|
  // |     +- GtkImage |url_image_|
  // +- GtkLabel |description_label|

  title_entry_ = gtk_entry_new();
  gtk_entry_set_activates_default(GTK_ENTRY(title_entry_), TRUE);
  g_signal_connect(title_entry_, "changed",
                   G_CALLBACK(OnEntryChangedThunk), this);
  accessible_widget_helper_->SetWidgetName(
      title_entry_,
      IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_LABEL);

  keyword_entry_ = gtk_entry_new();
  gtk_entry_set_activates_default(GTK_ENTRY(keyword_entry_), TRUE);
  g_signal_connect(keyword_entry_, "changed",
                   G_CALLBACK(OnEntryChangedThunk), this);
  g_signal_connect(keyword_entry_, "insert-text",
                   G_CALLBACK(LowercaseInsertTextHandler), NULL);
  accessible_widget_helper_->SetWidgetName(
      keyword_entry_,
      IDS_SEARCH_ENGINES_EDITOR_KEYWORD_LABEL);

  url_entry_ = gtk_entry_new();
  gtk_entry_set_activates_default(GTK_ENTRY(url_entry_), TRUE);
  g_signal_connect(url_entry_, "changed",
                   G_CALLBACK(OnEntryChangedThunk), this);
  accessible_widget_helper_->SetWidgetName(
      url_entry_,
      IDS_SEARCH_ENGINES_EDITOR_URL_LABEL);

  title_image_ = gtk_image_new_from_pixbuf(NULL);
  keyword_image_ = gtk_image_new_from_pixbuf(NULL);
  url_image_ = gtk_image_new_from_pixbuf(NULL);

  if (controller_->template_url()) {
    gtk_entry_set_text(
        GTK_ENTRY(title_entry_),
        UTF16ToUTF8(controller_->template_url()->short_name()).c_str());
    gtk_entry_set_text(
        GTK_ENTRY(keyword_entry_),
        UTF16ToUTF8(controller_->template_url()->keyword()).c_str());
    gtk_entry_set_text(
        GTK_ENTRY(url_entry_),
        GetDisplayURL(*controller_->template_url()).c_str());
    // We don't allow users to edit prepopulated URLs.
    gtk_editable_set_editable(
        GTK_EDITABLE(url_entry_),
        controller_->template_url()->prepopulate_id() == 0);

    if (controller_->template_url()->prepopulate_id() != 0) {
      GtkWidget* fake_label = gtk_label_new("Fake label");
      gtk_widget_set_sensitive(fake_label,
          controller_->template_url()->prepopulate_id() == 0);
      GtkStyle* label_style = gtk_widget_get_style(fake_label);
      GtkStyle* dialog_style = gtk_widget_get_style(dialog_);
      SetWidgetStyle(url_entry_, label_style, dialog_style);
      gtk_widget_destroy(fake_label);
    }
  }

  GtkWidget* controls = gtk_util::CreateLabeledControlsGroup(NULL,
      l10n_util::GetStringUTF8(
          IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_LABEL).c_str(),
      gtk_util::CreateEntryImageHBox(title_entry_, title_image_),
      l10n_util::GetStringUTF8(IDS_SEARCH_ENGINES_EDITOR_KEYWORD_LABEL).c_str(),
      gtk_util::CreateEntryImageHBox(keyword_entry_, keyword_image_),
      l10n_util::GetStringUTF8(IDS_SEARCH_ENGINES_EDITOR_URL_LABEL).c_str(),
      gtk_util::CreateEntryImageHBox(url_entry_, url_image_),
      NULL);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), controls,
                     FALSE, FALSE, 0);

  // On RTL UIs (such as Arabic and Hebrew) the description text is not
  // displayed correctly since it contains the substring "%s". This substring
  // is not interpreted by the Unicode BiDi algorithm as an LTR string and
  // therefore the end result is that the following right to left text is
  // displayed: ".three two s% one" (where 'one', 'two', etc. are words in
  // Hebrew).
  //
  // In order to fix this problem we transform the substring "%s" so that it
  // is displayed correctly when rendered in an RTL context.
  std::string description =
      l10n_util::GetStringUTF8(IDS_SEARCH_ENGINES_EDITOR_URL_DESCRIPTION_LABEL);
  if (base::i18n::IsRTL()) {
    const std::string reversed_percent("s%");
    std::string::size_type percent_index = description.find("%s");
    if (percent_index != std::string::npos) {
      description.replace(percent_index,
                          reversed_percent.length(),
                          reversed_percent);
    }
  }

  GtkWidget* description_label = gtk_label_new(description.c_str());
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), description_label,
                     FALSE, FALSE, 0);

  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  EnableControls();

  gtk_util::ShowDialog(dialog_);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroyThunk), this);
}

string16 EditSearchEngineDialog::GetTitleInput() const {
  return UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(title_entry_)));
}

string16 EditSearchEngineDialog::GetKeywordInput() const {
  return UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(keyword_entry_)));
}

std::string EditSearchEngineDialog::GetURLInput() const {
  return gtk_entry_get_text(GTK_ENTRY(url_entry_));
}

void EditSearchEngineDialog::EnableControls() {
  gtk_widget_set_sensitive(ok_button_,
                           controller_->IsKeywordValid(GetKeywordInput()) &&
                           controller_->IsTitleValid(GetTitleInput()) &&
                           controller_->IsURLValid(GetURLInput()));
  UpdateImage(keyword_image_, controller_->IsKeywordValid(GetKeywordInput()),
              IDS_SEARCH_ENGINES_INVALID_KEYWORD_TT);
  UpdateImage(url_image_, controller_->IsURLValid(GetURLInput()),
              IDS_SEARCH_ENGINES_INVALID_URL_TT);
  UpdateImage(title_image_, controller_->IsTitleValid(GetTitleInput()),
              IDS_SEARCH_ENGINES_INVALID_TITLE_TT);
}

void EditSearchEngineDialog::UpdateImage(GtkWidget* image,
                                         bool is_valid,
                                         int invalid_message_id) {
  if (is_valid) {
    gtk_widget_set_has_tooltip(image, FALSE);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image),
        ResourceBundle::GetSharedInstance().GetPixbufNamed(
            IDR_INPUT_GOOD));
  } else {
    gtk_widget_set_tooltip_text(
        image, l10n_util::GetStringUTF8(invalid_message_id).c_str());
    gtk_image_set_from_pixbuf(GTK_IMAGE(image),
        ResourceBundle::GetSharedInstance().GetPixbufNamed(
            IDR_INPUT_ALERT));
  }
}

void EditSearchEngineDialog::OnEntryChanged(GtkEditable* editable) {
  EnableControls();
}

void EditSearchEngineDialog::OnResponse(GtkDialog* dialog, int response_id) {
  if (response_id == GTK_RESPONSE_OK) {
    controller_->AcceptAddOrEdit(GetTitleInput(),
                                 GetKeywordInput(),
                                 GetURLInput());
  } else {
    controller_->CleanUpCancelledAdd();
  }
  gtk_widget_destroy(dialog_);
}

void EditSearchEngineDialog::OnWindowDestroy(GtkWidget* widget) {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}
