// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/first_run_bubble.h"

#include <gtk/gtk.h>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// Markup for the text of the Omnibox search label
const char kSearchLabelMarkup[] = "<big><b>%s</b></big>";

// Padding for the buttons on first run bubble.
const int kButtonPadding = 4;

// Padding between content and edge of bubble.
const int kContentBorder = 7;

// Vertical spacing between labels.
const int kInterLineSpacing = 5;

}  // namespace

// static
void FirstRunBubble::Show(Profile* profile,
                          GtkWidget* anchor,
                          const gfx::Rect& rect,
                          FirstRun::BubbleType bubble_type) {
  new FirstRunBubble(profile, anchor, rect, bubble_type);
}

void FirstRunBubble::BubbleClosing(BubbleGtk* bubble,
                                   bool closed_by_escape) {
  // TODO(port): Enable parent window
}

FirstRunBubble::FirstRunBubble(Profile* profile,
                               GtkWidget* anchor,
                               const gfx::Rect& rect,
                               FirstRun::BubbleType bubble_type)
    : profile_(profile),
      theme_service_(GtkThemeService::GetFrom(profile_)),
      anchor_(anchor),
      content_(NULL),
      bubble_(NULL) {
  content_ = gtk_vbox_new(FALSE, kInterLineSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(content_), kContentBorder);
  g_signal_connect(content_, "destroy",
                   G_CALLBACK(&HandleDestroyThunk), this);

  int width_resource = 0;
  std::vector<GtkWidget*> labels;
  if (bubble_type == FirstRun::LARGE_BUBBLE) {
    width_resource = IDS_FIRSTRUNBUBBLE_DIALOG_WIDTH_CHARS;
    InitializeContentForLarge(&labels);
  } else if (bubble_type == FirstRun::OEM_BUBBLE) {
    width_resource = IDS_FIRSTRUNOEMBUBBLE_DIALOG_WIDTH_CHARS;
    InitializeContentForOEM(&labels);
  } else if (bubble_type == FirstRun::MINIMAL_BUBBLE) {
    width_resource = IDS_FIRSTRUN_MINIMAL_BUBBLE_DIALOG_WIDTH_CHARS;
    InitializeContentForMinimal(&labels);
  } else {
    NOTREACHED();
  }

  InitializeLabels(width_resource, &labels);

  BubbleGtk::ArrowLocationGtk arrow_location =
      !base::i18n::IsRTL() ?
      BubbleGtk::ARROW_LOCATION_TOP_LEFT :
      BubbleGtk::ARROW_LOCATION_TOP_RIGHT;
  bubble_ = BubbleGtk::Show(anchor_,
                            &rect,
                            content_,
                            arrow_location,
                            true,  // match_system_theme
                            true,  // grab_input
                            theme_service_,
                            this);  // delegate
  if (!bubble_) {
    NOTREACHED();
    return;
  }
}

FirstRunBubble::~FirstRunBubble() {
}

void FirstRunBubble::InitializeContentForLarge(
    std::vector<GtkWidget*>* labels) {
  GtkWidget* label1 = theme_service_->BuildLabel("", gtk_util::kGdkBlack);
  labels->push_back(label1);
  char* markup = g_markup_printf_escaped(kSearchLabelMarkup,
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_TITLE).c_str());
  gtk_label_set_markup(GTK_LABEL(label1), markup);
  g_free(markup);

  GtkWidget* label2 = theme_service_->BuildLabel(
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_SUBTEXT), gtk_util::kGdkBlack);
  labels->push_back(label2);

  string16 search_engine = GetDefaultSearchEngineName(profile_);
  GtkWidget* label3 = theme_service_->BuildLabel(
      l10n_util::GetStringFUTF8(IDS_FR_BUBBLE_QUESTION, search_engine),
      gtk_util::kGdkBlack);
  labels->push_back(label3);

  GtkWidget* keep_button = gtk_button_new_with_label(
      l10n_util::GetStringFUTF8(IDS_FR_BUBBLE_OK, search_engine).c_str());
  GtkWidget* change_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_CHANGE).c_str());

  gtk_box_pack_start(GTK_BOX(content_), label1, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content_), label2, FALSE, FALSE, 0);
  // Leave an empty line.
  gtk_box_pack_start(GTK_BOX(content_), gtk_label_new(NULL), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content_), label3, FALSE, FALSE, 0);

  GtkWidget* bottom = gtk_hbox_new(FALSE, 0);
  // We want the buttons on the right, so just use an expanding label to fill
  // all of the extra space on the left.
  gtk_box_pack_start(GTK_BOX(bottom), gtk_label_new(NULL), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(bottom), keep_button, FALSE, FALSE,
                     kButtonPadding);
  gtk_box_pack_start(GTK_BOX(bottom), change_button, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(content_), bottom, FALSE, FALSE, 0);
  // We want the focus to start on the keep entry, not on the change button.
  gtk_widget_grab_focus(keep_button);

  g_signal_connect(keep_button, "clicked",
                   G_CALLBACK(&HandleKeepButtonThunk), this);
  g_signal_connect(change_button, "clicked",
                   G_CALLBACK(&HandleChangeButtonThunk), this);
}

void FirstRunBubble::InitializeContentForOEM(std::vector<GtkWidget*>* labels) {
  NOTIMPLEMENTED() << "Falling back to minimal bubble";
  InitializeContentForMinimal(labels);
}

void FirstRunBubble::InitializeContentForMinimal(
    std::vector<GtkWidget*>* labels) {
  GtkWidget* label1 = theme_service_->BuildLabel("", gtk_util::kGdkBlack);
  labels->push_back(label1);
  char* markup = g_markup_printf_escaped(kSearchLabelMarkup,
      l10n_util::GetStringFUTF8(
          IDS_FR_SE_BUBBLE_TITLE,
          GetDefaultSearchEngineName(profile_)).c_str());
  gtk_label_set_markup(GTK_LABEL(label1), markup);
  g_free(markup);

  GtkWidget* label2 = theme_service_->BuildLabel(
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_SUBTEXT), gtk_util::kGdkBlack);
  labels->push_back(label2);

  gtk_box_pack_start(GTK_BOX(content_), label1, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content_), label2, FALSE, FALSE, 0);
}

void FirstRunBubble::InitializeLabels(int width_resource,
                                      std::vector<GtkWidget*>* labels) {
  int width = -1;

  gtk_util::GetWidgetSizeFromResources(
      anchor_, width_resource, 0, &width, NULL);

  for (size_t i = 0; i < labels->size(); ++i) {
    // Resize the labels so that they don't wrap more than necessary.  We leave
    // |content_| unsized so that it'll expand as needed to hold the other
    // widgets -- the buttons may be wider than |width| on high-DPI displays.
    gtk_util::SetLabelWidth((*labels)[i], width);
  }
}

void FirstRunBubble::HandleDestroy(GtkWidget* sender) {
  content_ = NULL;
  delete this;
}

void FirstRunBubble::HandleKeepButton(GtkWidget* sender) {
  bubble_->Close();
}

void FirstRunBubble::HandleChangeButton(GtkWidget* sender) {
  bubble_->Close();
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  DCHECK(browser);
  browser->OpenSearchEngineOptionsDialog();
}
