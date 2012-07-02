// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/zoom_bubble_gtk.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"

namespace {

// Pointer to singleton object (NULL if no bubble is open).
ZoomBubbleGtk* g_bubble = NULL;

// Padding on left and right of bubble.
const int kContentBorder = 7;

// Number of milliseconds the bubble should stay open for if it will auto-close.
const int kBubbleCloseDelay = 400;

// Need to manually set anchor width and height to ensure that the bubble shows
// in the correct spot the first time it is displayed when no icon is present.
const int kBubbleAnchorWidth = 20;
const int kBubbleAnchorHeight = 25;

}  // namespace

// static
void ZoomBubbleGtk::Show(GtkWidget* anchor,
                         Profile* profile,
                         int zoom_percent,
                         bool auto_close) {
  // If the bubble is already showing and its |auto_close_| value is equal to
  // |auto_close|, the bubble can be reused and only the label text needs to
  // be updated.
  if (g_bubble &&
      g_bubble->auto_close_ == auto_close &&
      g_bubble->bubble_->anchor_widget() == anchor) {
    string16 text = l10n_util::GetStringFUTF16Int(
        IDS_ZOOM_PERCENT, zoom_percent);
    gtk_label_set_text(GTK_LABEL(g_bubble->label_), UTF16ToUTF8(text).c_str());

    if (auto_close) {
      // If the bubble should be closed automatically, reset the timer so that
      // it will show for the full amount of time instead of only what remained
      // from the previous time.
      g_bubble->timer_.Reset();
    }
  } else {
    // If the bubble is already showing but its |auto_close_| value is not equal
    // to |auto_close|, the bubble's focus properties must change, so the
    // current bubble must be closed and a new one created.
    if (g_bubble)
      g_bubble->Close();

    g_bubble = new ZoomBubbleGtk(anchor, profile, zoom_percent, auto_close);
  }
}

// static
void ZoomBubbleGtk::Close() {
  if (g_bubble)
    g_bubble->CloseBubble();
}

void ZoomBubbleGtk::BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) {
}

void ZoomBubbleGtk::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_THEME_CHANGED);

  gtk_widget_modify_fg(label_,
                       GTK_STATE_NORMAL,
                       theme_service_->UsingNativeTheme() ?
                           NULL : &ui::kGdkBlack);
}

ZoomBubbleGtk::ZoomBubbleGtk(GtkWidget* anchor,
                             Profile* profile,
                             int zoom_percent,
                             bool auto_close)
    : auto_close_(auto_close),
      theme_service_(GtkThemeService::GetFrom(profile)) {
  string16 text = l10n_util::GetStringFUTF16Int(
      IDS_ZOOM_PERCENT, zoom_percent);
  label_ = gtk_label_new(UTF16ToUTF8(text).c_str());

  GtkWidget* alignment = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0,
                            kContentBorder, kContentBorder);
  gtk_container_add(GTK_CONTAINER(alignment), label_);

  GtkWidget* content = gtk_event_box_new();
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(content), TRUE);
  gtk_container_add(GTK_CONTAINER(content), alignment);
  gtk_widget_show_all(content);
  gfx::Rect rect(kBubbleAnchorWidth, kBubbleAnchorHeight);
  BubbleGtk::ArrowLocationGtk arrow_location =
      BubbleGtk::ARROW_LOCATION_TOP_MIDDLE;
  int attributeFlags = BubbleGtk::MATCH_SYSTEM_THEME | BubbleGtk::POPUP_WINDOW;
  bubble_ = BubbleGtk::Show(anchor,
                            &rect,
                            content,
                            arrow_location,
                            auto_close ? attributeFlags :
                                attributeFlags | BubbleGtk::GRAB_INPUT,
                            theme_service_,
                            this);  // delegate

  if (!bubble_) {
    NOTREACHED();
    return;
  }

  if (auto_close) {
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kBubbleCloseDelay),
        this,
        &ZoomBubbleGtk::CloseBubble);
  }

  g_signal_connect(content, "destroy",
                   G_CALLBACK(&OnDestroyThunk), this);

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
  theme_service_->InitThemesFor(this);
}

ZoomBubbleGtk::~ZoomBubbleGtk() {
  DCHECK_EQ(g_bubble, this);
  // Set singleton pointer to NULL.
  g_bubble = NULL;
}

void ZoomBubbleGtk::CloseBubble() {
  bubble_->Close();
}

void ZoomBubbleGtk::OnDestroy(GtkWidget* widget) {
  // Listen to the destroy signal and delete this instance when it is caught.
  delete this;
}
