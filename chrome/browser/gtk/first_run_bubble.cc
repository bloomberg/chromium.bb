// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/first_run_bubble.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"

namespace {
// Markup for the text of the Omnibox search label
const char kSearchLabelMarkup[] = "<big><b>%s</b></big>";

// Padding for the buttons on first run bubble.
const int kButtonPadding = 4;

string16 GetDefaultSearchEngineName(Profile* profile) {
  if (!profile) {
    NOTREACHED();
    return string16();
  }
  const TemplateURL* const default_provider =
      profile->GetTemplateURLModel()->GetDefaultSearchProvider();
  if (!default_provider) {
    return string16();
  }
  return WideToUTF16(default_provider->short_name());
}
}  // namespace

// static
void FirstRunBubble::Show(Profile* profile,
                          GtkWindow* parent,
                          const gfx::Rect& rect,
                          bool use_OEM_bubble) {
  new FirstRunBubble(profile, parent, rect);
}

void FirstRunBubble::InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                       bool closed_by_escape) {
  // TODO(port): Enable parent window
}

FirstRunBubble::FirstRunBubble(Profile* profile,
                               GtkWindow* parent,
                               const gfx::Rect& rect)
    : profile_(profile),
      parent_(parent),
      content_(NULL),
      bubble_(NULL) {
  GtkWidget* label1 = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(kSearchLabelMarkup,
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_TITLE).c_str());
  gtk_label_set_markup(GTK_LABEL(label1), markup);
  g_free(markup);
  gtk_misc_set_alignment(GTK_MISC(label1), 0, .5);

  GtkWidget* label2 = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_SUBTEXT).c_str());
  gtk_misc_set_alignment(GTK_MISC(label2), 0, .5);
  gtk_label_set_line_wrap(GTK_LABEL(label2), TRUE);

  string16 search_engine = GetDefaultSearchEngineName(profile_);
  GtkWidget* label3 = gtk_label_new(
      l10n_util::GetStringFUTF8(IDS_FR_BUBBLE_QUESTION, search_engine).c_str());
  gtk_misc_set_alignment(GTK_MISC(label3), 0, .5);
  gtk_label_set_line_wrap(GTK_LABEL(label3), TRUE);

  GtkWidget* keep_button = gtk_button_new_with_label(
      l10n_util::GetStringFUTF8(IDS_FR_BUBBLE_OK, search_engine).c_str());
  GtkWidget* change_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_CHANGE).c_str());

  content_ = gtk_vbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(content_), label1, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content_), label2, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content_), label3, TRUE, TRUE, 0);

  GtkWidget* bottom = gtk_hbox_new(FALSE, 0);
  // We want the buttons on the right, so just use an expanding label to fill
  // all of the extra space on the right.
  gtk_box_pack_start(GTK_BOX(bottom), gtk_label_new(""), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(bottom), keep_button, FALSE, FALSE,
                     kButtonPadding);
  gtk_box_pack_start(GTK_BOX(bottom), change_button, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(content_), bottom, TRUE, TRUE, 0);
  // We want the focus to start on the keep entry, not on the change button.
  gtk_container_set_focus_child(GTK_CONTAINER(content_), keep_button);

  bubble_ = InfoBubbleGtk::Show(parent_, rect, content_, this);
  if (!bubble_) {
    NOTREACHED();
    return;
  }

  g_signal_connect(content_, "destroy",
                   G_CALLBACK(&HandleDestroyThunk), this);
  g_signal_connect(keep_button, "clicked",
                   G_CALLBACK(&HandleKeepButtonThunk), this);
  g_signal_connect(change_button, "clicked",
                   G_CALLBACK(&HandleChangeButtonThunk), this);
}

void FirstRunBubble::HandleChangeButton() {
  bubble_->Close();
  ShowOptionsWindow(OPTIONS_PAGE_GENERAL, OPTIONS_GROUP_DEFAULT_SEARCH,
                    profile_);
}

void FirstRunBubble::HandleDestroy() {
  content_ = NULL;
  delete this;
}

void FirstRunBubble::HandleKeepButton() {
  bubble_->Close();
}
