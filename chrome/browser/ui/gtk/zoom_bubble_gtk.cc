// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/zoom_bubble_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"

namespace {

// Pointer to singleton object (NULL if no bubble is open).
ZoomBubbleGtk* g_bubble = NULL;

// Padding on each side of the percentage label and the left and right sides of
// the "Set to default" button.
const int kSidePadding = 5;

// Number of milliseconds the bubble should stay open for if it will auto-close.
const int kBubbleCloseDelay = 1500;

// Need to manually set anchor width and height to ensure that the bubble shows
// in the correct spot the first time it is displayed when no icon is present.
const int kBubbleAnchorWidth = 20;
const int kBubbleAnchorHeight = 25;

}  // namespace

// static
void ZoomBubbleGtk::Show(GtkWidget* anchor,
                         TabContents* tab_contents,
                         bool auto_close) {
  // If the bubble is already showing and its |auto_close_| value is equal to
  // |auto_close|, the bubble can be reused and only the label text needs to
  // be updated.
  if (g_bubble &&
      g_bubble->auto_close_ == auto_close &&
      g_bubble->bubble_->anchor_widget() == anchor) {
    g_bubble->Refresh();
  } else {
    // If the bubble is already showing but its |auto_close_| value is not equal
    // to |auto_close|, the bubble's focus properties must change, so the
    // current bubble must be closed and a new one created.
    if (g_bubble)
      g_bubble->Close();

    g_bubble = new ZoomBubbleGtk(anchor, tab_contents, auto_close);
  }
}

// static
void ZoomBubbleGtk::Close() {
  if (g_bubble)
    g_bubble->CloseBubble();
}

ZoomBubbleGtk::ZoomBubbleGtk(GtkWidget* anchor,
                             TabContents* tab_contents,
                             bool auto_close)
    : auto_close_(auto_close),
      mouse_inside_(false),
      tab_contents_(tab_contents) {
  GtkThemeService* theme_service =
      GtkThemeService::GetFrom(Profile::FromBrowserContext(
          tab_contents->web_contents()->GetBrowserContext()));

  event_box_ = gtk_event_box_new();
  GtkWidget* container = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(event_box_), container);

  int zoom_percent = tab_contents->zoom_controller()->zoom_percent();
  std::string percentage_text = UTF16ToUTF8(l10n_util::GetStringFUTF16Int(
      IDS_TOOLTIP_ZOOM, zoom_percent));
  label_ = theme_service->BuildLabel(percentage_text, ui::kGdkBlack);
  gtk_widget_modify_font(label_, pango_font_description_from_string("13"));
  gtk_misc_set_padding(GTK_MISC(label_), kSidePadding, kSidePadding);

  gtk_box_pack_start(GTK_BOX(container), label_, FALSE, FALSE, 0);

  GtkWidget* separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(container), separator, FALSE, FALSE, 0);

  GtkWidget* set_default_button = theme_service->BuildChromeButton();
  gtk_button_set_label(GTK_BUTTON(set_default_button),
      l10n_util::GetStringUTF8(IDS_ZOOM_SET_DEFAULT).c_str());

  GtkWidget* alignment = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(
      GTK_ALIGNMENT(alignment), 0, 0, kSidePadding, kSidePadding);
  gtk_container_add(GTK_CONTAINER(alignment), set_default_button);

  gtk_box_pack_start(GTK_BOX(container), alignment, FALSE, FALSE, 0);
  g_signal_connect(set_default_button, "clicked",
                   G_CALLBACK(&OnSetDefaultLinkClickThunk), this);

  gtk_container_set_focus_child(GTK_CONTAINER(container), NULL);

  gfx::Rect rect(kBubbleAnchorWidth, kBubbleAnchorHeight);
  BubbleGtk::ArrowLocationGtk arrow_location =
      BubbleGtk::ARROW_LOCATION_TOP_MIDDLE;
  int bubble_options = BubbleGtk::MATCH_SYSTEM_THEME | BubbleGtk::POPUP_WINDOW;
  bubble_ = BubbleGtk::Show(anchor, &rect, event_box_, arrow_location,
      auto_close ? bubble_options : bubble_options | BubbleGtk::GRAB_INPUT,
      theme_service, NULL);

  if (!bubble_) {
    NOTREACHED();
    return;
  }

  g_signal_connect(event_box_, "destroy",
                   G_CALLBACK(&OnDestroyThunk), this);

  if (auto_close_) {
    // If this is an auto-closing bubble, listen to leave/enter to keep the
    // bubble alive if the mouse stays anywhere inside the bubble.
    gtk_widget_add_events(event_box_, GDK_ENTER_NOTIFY_MASK |
                                      GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect_after(event_box_, "enter-notify-event",
                           G_CALLBACK(&OnMouseEnterThunk), this);
    g_signal_connect(event_box_, "leave-notify-event",
                     G_CALLBACK(&OnMouseLeaveThunk), this);

    // This is required as a leave is fired when the mouse goes from inside the
    // bubble's container to inside the set default button.
    gtk_widget_add_events(set_default_button, GDK_ENTER_NOTIFY_MASK);
    g_signal_connect_after(set_default_button, "enter-notify-event",
                           G_CALLBACK(&OnMouseEnterThunk), this);
  }

  StartTimerIfNecessary();
}

ZoomBubbleGtk::~ZoomBubbleGtk() {
  DCHECK_EQ(g_bubble, this);
  // Set singleton pointer to NULL.
  g_bubble = NULL;
}

void ZoomBubbleGtk::Refresh() {
  int zoom_percent = tab_contents_->zoom_controller()->zoom_percent();
  string16 text =
      l10n_util::GetStringFUTF16Int(IDS_TOOLTIP_ZOOM, zoom_percent);
  gtk_label_set_text(GTK_LABEL(g_bubble->label_), UTF16ToUTF8(text).c_str());
  StartTimerIfNecessary();
}

void ZoomBubbleGtk::StartTimerIfNecessary() {
  if (!auto_close_ || mouse_inside_)
    return;

  if (timer_.IsRunning()) {
    timer_.Reset();
  } else {
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kBubbleCloseDelay),
        this,
        &ZoomBubbleGtk::CloseBubble);
  }
}

void ZoomBubbleGtk::StopTimerIfNecessary() {
  if (timer_.IsRunning())
    timer_.Stop();
}

void ZoomBubbleGtk::CloseBubble() {
  DCHECK(bubble_);
  bubble_->Close();
}

void ZoomBubbleGtk::OnDestroy(GtkWidget* widget) {
  // Listen to the destroy signal and delete this instance when it is caught.
  delete this;
}

void ZoomBubbleGtk::OnSetDefaultLinkClick(GtkWidget* widget) {
  double default_zoom_level = Profile::FromBrowserContext(
      tab_contents_->web_contents()->GetBrowserContext())->
          GetPrefs()->GetDouble(prefs::kDefaultZoomLevel);
  tab_contents_->web_contents()->GetRenderViewHost()->
      SetZoomLevel(default_zoom_level);
}

gboolean ZoomBubbleGtk::OnMouseEnter(GtkWidget* widget,
                                     GdkEventCrossing* event) {
  mouse_inside_ = true;
  StopTimerIfNecessary();
  return FALSE;
}

gboolean ZoomBubbleGtk::OnMouseLeave(GtkWidget* widget,
                                     GdkEventCrossing* event) {
  mouse_inside_ = false;
  StartTimerIfNecessary();
  return FALSE;
}
