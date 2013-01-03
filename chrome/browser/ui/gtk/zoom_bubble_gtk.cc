// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/zoom_bubble_gtk.h"

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"

namespace {

// Pointer to singleton object (NULL if no bubble is open).
ZoomBubbleGtk* g_bubble = NULL;

// Number of milliseconds the bubble should stay open for if it will auto-close.
const int kBubbleCloseDelay = 1500;

// Need to manually set anchor width and height to ensure that the bubble shows
// in the correct spot the first time it is displayed when no icon is present.
const int kBubbleAnchorWidth = 20;
const int kBubbleAnchorHeight = 25;

}  // namespace

// static
void ZoomBubbleGtk::ShowBubble(content::WebContents* web_contents,
                               bool auto_close) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser && browser->window() && browser->fullscreen_controller());

  LocationBar* location_bar = browser->window()->GetLocationBar();
  GtkWidget* anchor = browser->window()->IsFullscreen() ?
      GTK_WIDGET(browser->window()->GetNativeWindow()) :
      static_cast<LocationBarViewGtk*>(location_bar)->zoom_widget();

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
    CloseBubble();
    DCHECK(!g_bubble);

    g_bubble = new ZoomBubbleGtk(anchor,
                                 web_contents,
                                 auto_close,
                                 browser->fullscreen_controller());
  }
}

// static
void ZoomBubbleGtk::CloseBubble() {
  if (g_bubble)
    g_bubble->Close();
}

// static
bool ZoomBubbleGtk::IsShowing() {
  return g_bubble != NULL;
}

ZoomBubbleGtk::ZoomBubbleGtk(GtkWidget* anchor,
                             content::WebContents* web_contents,
                             bool auto_close,
                             FullscreenController* fullscreen_controller)
    : auto_close_(auto_close),
      mouse_inside_(false),
      web_contents_(web_contents) {
  GtkThemeService* theme_service =
      GtkThemeService::GetFrom(Profile::FromBrowserContext(
          web_contents_->GetBrowserContext()));

  event_box_ = gtk_event_box_new();
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_), FALSE);
  GtkWidget* container = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(event_box_), container);

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents_);
  int zoom_percent = zoom_controller->zoom_percent();
  std::string percentage_text = UTF16ToUTF8(l10n_util::GetStringFUTF16Int(
      IDS_TOOLTIP_ZOOM, zoom_percent));
  label_ = theme_service->BuildLabel(percentage_text, ui::kGdkBlack);
  gtk_widget_modify_font(label_, pango_font_description_from_string("13"));
  gtk_misc_set_padding(GTK_MISC(label_),
                       ui::kControlSpacing, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(container), label_, FALSE, FALSE, 0);

  GtkWidget* set_default_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_ZOOM_SET_DEFAULT).c_str());

  GtkWidget* alignment = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
                            0,
                            ui::kControlSpacing,
                            ui::kControlSpacing,
                            ui::kControlSpacing);
  gtk_container_add(GTK_CONTAINER(alignment), set_default_button);

  gtk_box_pack_start(GTK_BOX(container), alignment, FALSE, FALSE, 0);

  g_signal_connect(set_default_button, "clicked",
                   G_CALLBACK(&OnSetDefaultLinkClickThunk), this);

  gtk_container_set_focus_child(GTK_CONTAINER(container), NULL);

  gfx::Rect rect = gfx::Rect(kBubbleAnchorWidth, kBubbleAnchorHeight);
  BubbleGtk::FrameStyle frame_style = gtk_widget_is_toplevel(anchor) ?
      BubbleGtk::FIXED_TOP_RIGHT : BubbleGtk::ANCHOR_TOP_MIDDLE;
  int bubble_options = BubbleGtk::MATCH_SYSTEM_THEME | BubbleGtk::POPUP_WINDOW;
  bubble_ = BubbleGtk::Show(anchor, &rect, event_box_, frame_style,
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
    g_signal_connect_after(event_box_, "enter-notify-event",
                           G_CALLBACK(&OnMouseEnterThunk), this);
    g_signal_connect(event_box_, "leave-notify-event",
                     G_CALLBACK(&OnMouseLeaveThunk), this);

    // This is required as a leave is fired when the mouse goes from inside the
    // bubble's container to inside the set default button.
    gtk_widget_add_events(set_default_button, GDK_ENTER_NOTIFY_MASK);
    g_signal_connect_after(set_default_button, "enter-notify-event",
                           G_CALLBACK(&OnMouseEnterThunk), this);
    g_signal_connect(set_default_button, "leave-notify-event",
                     G_CALLBACK(&OnMouseLeaveThunk), this);
  }

  registrar_.Add(this,
                 chrome::NOTIFICATION_FULLSCREEN_CHANGED,
                 content::Source<FullscreenController>(fullscreen_controller));

  StartTimerIfNecessary();
}

ZoomBubbleGtk::~ZoomBubbleGtk() {
  DCHECK_EQ(g_bubble, this);
  // Set singleton pointer to NULL.
  g_bubble = NULL;
}

void ZoomBubbleGtk::Refresh() {
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents_);
  int zoom_percent = zoom_controller->zoom_percent();
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
        &ZoomBubbleGtk::Close);
  }
}

void ZoomBubbleGtk::StopTimerIfNecessary() {
  if (timer_.IsRunning())
    timer_.Stop();
}

void ZoomBubbleGtk::Close() {
  DCHECK(bubble_);
  bubble_->Close();
}

void ZoomBubbleGtk::OnDestroy(GtkWidget* widget) {
  // Listen to the destroy signal and delete this instance when it is caught.
  delete this;
}

void ZoomBubbleGtk::OnSetDefaultLinkClick(GtkWidget* widget) {
  double default_zoom_level = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext())->GetPrefs()->GetDouble(
          prefs::kDefaultZoomLevel);
  web_contents_->GetRenderViewHost()->SetZoomLevel(default_zoom_level);
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

void ZoomBubbleGtk::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_FULLSCREEN_CHANGED);
  CloseBubble();
}
