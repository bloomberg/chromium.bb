// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/first_run_bubble.h"

#include <gtk/gtk.h>

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Markup for the text of the Omnibox search label
const char kSearchLabelMarkup[] = "<big><b>%s</b></big>";

}  // namespace

// static
void FirstRunBubble::Show(Browser* browser,
                          GtkWidget* anchor,
                          const gfx::Rect& rect) {
  first_run::LogFirstRunMetric(first_run::FIRST_RUN_BUBBLE_SHOWN);

  new FirstRunBubble(browser, anchor, rect);
}

void FirstRunBubble::BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) {
  // TODO(port): Enable parent window
}

FirstRunBubble::FirstRunBubble(Browser* browser,
                               GtkWidget* anchor,
                               const gfx::Rect& rect)
    : browser_(browser),
      bubble_(NULL) {
  GtkThemeService* theme_service = GtkThemeService::GetFrom(browser->profile());
  GtkWidget* title = theme_service->BuildLabel(std::string(), ui::kGdkBlack);
  char* markup = g_markup_printf_escaped(
      kSearchLabelMarkup,
      l10n_util::GetStringFUTF8(IDS_FR_BUBBLE_TITLE,
                                GetDefaultSearchEngineName(browser->profile()))
          .c_str());
  gtk_label_set_markup(GTK_LABEL(title), markup);
  g_free(markup);

  GtkWidget* change = theme_service->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_CHANGE));
  g_signal_connect(change, "clicked", G_CALLBACK(&HandleChangeLinkThunk), this);

  GtkWidget* subtext = theme_service->BuildLabel(
      l10n_util::GetStringUTF8(IDS_FR_BUBBLE_SUBTEXT), ui::kGdkBlack);

  GtkWidget* top_line = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(top_line), title, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(top_line), change, FALSE, FALSE, 0);

  GtkWidget* content = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(content),
                                 ui::kContentAreaBorder);
  g_signal_connect(content, "destroy", G_CALLBACK(&HandleDestroyThunk), this);
  gtk_box_pack_start(GTK_BOX(content), top_line, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content), subtext, FALSE, FALSE, 0);

  bubble_ = BubbleGtk::Show(anchor,
                            &rect,
                            content,
                            BubbleGtk::ANCHOR_TOP_LEFT,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_service,
                            this);
  DCHECK(bubble_);
}

FirstRunBubble::~FirstRunBubble() {
}

void FirstRunBubble::HandleDestroy(GtkWidget* sender) {
  delete this;
}

void FirstRunBubble::HandleChangeLink(GtkWidget* sender) {
  first_run::LogFirstRunMetric(first_run::FIRST_RUN_BUBBLE_CHANGE_INVOKED);

  // Cache browser_ before closing the bubble, which deletes |this|.
  Browser* browser = browser_;
  bubble_->Close();
  if (browser)
    chrome::ShowSearchEngineSettings(browser);
}
