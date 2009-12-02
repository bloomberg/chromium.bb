// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/first_run_bubble.h"

#include <gtk/gtk.h>

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

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

void FirstRunBubble::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_THEME_CHANGED);

  if (theme_provider_->UseGtkTheme()) {
    for (std::vector<GtkWidget*>::iterator it = labels_.begin();
         it != labels_.end(); ++it) {
      gtk_widget_modify_fg(*it, GTK_STATE_NORMAL, NULL);
    }
  } else {
    for (std::vector<GtkWidget*>::iterator it = labels_.begin();
         it != labels_.end(); ++it) {
      gtk_widget_modify_fg(*it, GTK_STATE_NORMAL, &gfx::kGdkBlack);
    }
  }
}

FirstRunBubble::FirstRunBubble(Profile* profile,
                               GtkWindow* parent,
                               const gfx::Rect& rect)
    : profile_(profile),
      theme_provider_(GtkThemeProvider::GetFrom(profile_)),
      parent_(parent),
      content_(NULL),
      bubble_(NULL) {
  GtkWidget* label1 = gtk_label_new(NULL);
  labels_.push_back(label1);
  char* markup = g_markup_printf_escaped(kSearchLabelMarkup,
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_TITLE).c_str());
  gtk_label_set_markup(GTK_LABEL(label1), markup);
  g_free(markup);
  gtk_misc_set_alignment(GTK_MISC(label1), 0, .5);
  // TODO(erg): Theme these colors.
  gtk_widget_modify_fg(label1, GTK_STATE_NORMAL, &gfx::kGdkBlack);

  GtkWidget* label2 = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_SUBTEXT).c_str());
  labels_.push_back(label2);
  gtk_misc_set_alignment(GTK_MISC(label2), 0, .5);
  gtk_label_set_line_wrap(GTK_LABEL(label2), TRUE);
  gtk_widget_modify_fg(label2, GTK_STATE_NORMAL, &gfx::kGdkBlack);

  string16 search_engine = GetDefaultSearchEngineName(profile_);
  GtkWidget* label3 = gtk_label_new(
      l10n_util::GetStringFUTF8(IDS_FR_BUBBLE_QUESTION, search_engine).c_str());
  labels_.push_back(label3);
  gtk_misc_set_alignment(GTK_MISC(label3), 0, .5);
  gtk_label_set_line_wrap(GTK_LABEL(label3), TRUE);
  gtk_widget_modify_fg(label3, GTK_STATE_NORMAL, &gfx::kGdkBlack);

  GtkWidget* keep_button = gtk_button_new_with_label(
      l10n_util::GetStringFUTF8(IDS_FR_BUBBLE_OK, search_engine).c_str());
  GtkWidget* change_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_CHANGE).c_str());

  content_ = gtk_vbox_new(FALSE, 5);

  // We compute the widget's size using the parent window -- |content_| is
  // unrealized at this point.
  int width = -1, height = -1;
  gtk_util::GetWidgetSizeFromResources(
      GTK_WIDGET(parent_),
      IDS_FIRSTRUNBUBBLE_DIALOG_WIDTH_CHARS,
      IDS_FIRSTRUNBUBBLE_DIALOG_HEIGHT_LINES,
      &width, &height);
  // Resize the labels so that they don't wrap more than necessary.  We leave
  // |content_| unsized so that it'll expand as needed to hold the other
  // widgets -- the buttons may be wider than |width| on high-DPI displays.
  gtk_widget_set_size_request(label1, width, -1);
  gtk_widget_set_size_request(label2, width, -1);
  gtk_widget_set_size_request(label3, width, -1);

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

  InfoBubbleGtk::ArrowLocationGtk arrow_location =
      (l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT) ?
      InfoBubbleGtk::ARROW_LOCATION_TOP_LEFT :
      InfoBubbleGtk::ARROW_LOCATION_TOP_RIGHT;
  bubble_ = InfoBubbleGtk::Show(parent_,
                                rect,
                                content_,
                                arrow_location,
                                true,
                                theme_provider_,
                                this);  // delegate
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

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  theme_provider_->InitThemesFor(this);
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
