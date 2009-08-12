// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/gtk/blocked_popup_container_view_gtk.h"

#include "app/l10n_util.h"
#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {
// The minimal border around the edge of the notification.
const int kSmallPadding = 2;

// Color of the border.
const GdkColor kBorderColor = GDK_COLOR_RGB(190, 205, 223);

// Color of the gradient in the background.
const double kBackgroundColorTop[] = { 246.0 / 255, 250.0 / 255, 1.0 };
const double kBackgroundColorBottom[] = { 219.0 / 255, 235.0 / 255, 1.0 };

// Rounded corner radius (in pixels).
const int kCornerSize = 4;

enum FrameType {
  FRAME_MASK,
  FRAME_STROKE,
};

std::vector<GdkPoint> MakeFramePolygonPoints(int width,
                                             int height,
                                             FrameType type) {
  using gtk_util::MakeBidiGdkPoint;
  std::vector<GdkPoint> points;

  bool ltr = l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT;
  // If we have a stroke, we have to offset some of our points by 1 pixel.
  // We have to inset by 1 pixel when we draw horizontal lines that are on the
  // bottom or when we draw vertical lines that are closer to the end (end is
  // right for ltr).
  int y_off = (type == FRAME_MASK) ? 0 : -1;
  // We use this one for LTR.
  int x_off_l = ltr ? y_off : 0;
  // We use this one for RTL.
  int x_off_r = !ltr ? -y_off : 0;

  // Bottom left corner.
  points.push_back(MakeBidiGdkPoint(0, height + y_off, width, ltr));

  // Top left (rounded) corner.
  points.push_back(MakeBidiGdkPoint(x_off_r, kCornerSize - 1, width, ltr));
  points.push_back(MakeBidiGdkPoint(kCornerSize + x_off_r - 1, 0, width, ltr));

  // Top right (rounded) corner.
  points.push_back(MakeBidiGdkPoint(
      width - kCornerSize + 1 + x_off_l, 0, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      width + x_off_l, kCornerSize - 1, width, ltr));

  // Bottom right corner.
  points.push_back(MakeBidiGdkPoint(
      width + x_off_l, height + y_off, width, ltr));

  return points;
}

}  // namespace

// static
BlockedPopupContainerView* BlockedPopupContainerView::Create(
    BlockedPopupContainer* container) {
  return new BlockedPopupContainerViewGtk(container);
}

BlockedPopupContainerViewGtk::~BlockedPopupContainerViewGtk() {
  container_.Destroy();
}

TabContentsViewGtk* BlockedPopupContainerViewGtk::ContainingView() {
  return static_cast<TabContentsViewGtk*>(
      model_->GetConstrainingContents(NULL)->view());
}

void BlockedPopupContainerViewGtk::GetURLAndTitleForPopup(
    size_t index, string16* url, string16* title) const {
  DCHECK(url);
  DCHECK(title);
  TabContents* tab_contents = model_->GetTabContentsAt(index);
  const GURL& tab_contents_url = tab_contents->GetURL().GetOrigin();
  *url = UTF8ToUTF16(tab_contents_url.possibly_invalid_spec());
  *title = tab_contents->GetTitle();
}

// Overridden from BlockedPopupContainerView:
void BlockedPopupContainerViewGtk::SetPosition() {
  // No-op. Not required with the GTK version.
}

void BlockedPopupContainerViewGtk::ShowView() {
  // TODO(erg): Animate in.
  gtk_widget_show_all(container_.get());
}

void BlockedPopupContainerViewGtk::UpdateLabel() {
  size_t blocked_popups = model_->GetBlockedPopupCount();

  GtkWidget* label = gtk_bin_get_child(GTK_BIN(menu_button_));
  if (!label) {
    label = gtk_label_new("");
    gtk_container_add(GTK_CONTAINER(menu_button_), label);
  }

  gtk_label_set_text(
      GTK_LABEL(label),
      (blocked_popups > 0) ?
      l10n_util::GetStringFUTF8(IDS_POPUPS_BLOCKED_COUNT,
                                UintToString16(blocked_popups)).c_str() :
      l10n_util::GetStringUTF8(IDS_POPUPS_UNBLOCKED).c_str());
}

void BlockedPopupContainerViewGtk::HideView() {
  // TODO(erg): Animate out.
  gtk_widget_hide(container_.get());
}

void BlockedPopupContainerViewGtk::Destroy() {
  ContainingView()->RemoveBlockedPopupView(this);
  delete this;
}

void BlockedPopupContainerViewGtk::Observe(NotificationType type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_THEME_CHANGED);

  // Make sure the label exists (so we can change its colors).
  UpdateLabel();

  // Update the label's colors.
  GtkWidget* label = gtk_bin_get_child(GTK_BIN(menu_button_));
  if (theme_provider_->UseGtkTheme()) {
    gtk_util::SetLabelColor(label, NULL);
  } else {
    GdkColor color = theme_provider_->GetGdkColor(
        BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
    gtk_util::SetLabelColor(label, &color);
  }
}

bool BlockedPopupContainerViewGtk::IsCommandEnabled(int command_id) const {
  return true;
}

bool BlockedPopupContainerViewGtk::IsItemChecked(int id) const {
  DCHECK_GT(id, 0);
  size_t id_size_t = static_cast<size_t>(id);
  if (id_size_t > BlockedPopupContainer::kImpossibleNumberOfPopups) {
    return model_->IsHostWhitelisted(
        id_size_t - BlockedPopupContainer::kImpossibleNumberOfPopups - 1);
  }

  return false;
}

void BlockedPopupContainerViewGtk::ExecuteCommand(int id) {
  DCHECK_GT(id, 0);
  size_t id_size_t = static_cast<size_t>(id);
  if (id_size_t > BlockedPopupContainer::kImpossibleNumberOfPopups) {
    // Decrement id since all index based commands have 1 added to them. (See
    // ButtonPressed() for detail).
    model_->ToggleWhitelistingForHost(
        id_size_t - BlockedPopupContainer::kImpossibleNumberOfPopups - 1);
  } else {
    model_->LaunchPopupAtIndex(id_size_t - 1);
  }
}

BlockedPopupContainerViewGtk::BlockedPopupContainerViewGtk(
    BlockedPopupContainer* container)
    : model_(container),
      theme_provider_(GtkThemeProvider::GetFrom(container->profile())),
      close_button_(CustomDrawButton::CloseButton(theme_provider_)),
      notification_width_(-1),
      notification_height_(-1) {
  Init();

  registrar_.Add(this,
                 NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  theme_provider_->InitThemesFor(this);
}

void BlockedPopupContainerViewGtk::Init() {
  menu_button_ = theme_provider_->BuildChromeButton();
  UpdateLabel();
  g_signal_connect(menu_button_, "clicked",
                   G_CALLBACK(OnMenuButtonClicked), this);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), menu_button_, FALSE, FALSE, kSmallPadding);
  gtk_util::CenterWidgetInHBox(hbox, close_button_->widget(), true, 0);
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnCloseButtonClicked), this);

  container_.Own(gtk_util::CreateGtkBorderBin(hbox, NULL,
      kSmallPadding, kSmallPadding, kSmallPadding, kSmallPadding));
  // Manually paint the event box.
  gtk_widget_set_app_paintable(container_.get(), TRUE);
  g_signal_connect(container_.get(), "expose-event",
                   G_CALLBACK(OnContainerExpose), this);

  ContainingView()->AttachBlockedPopupView(this);
}

void BlockedPopupContainerViewGtk::OnMenuButtonClicked(
    GtkButton *button, BlockedPopupContainerViewGtk* container) {
  container->launch_menu_.reset(new MenuGtk(container, false));

  // Set items 1 .. popup_count as individual popups.
  size_t popup_count = container->model_->GetBlockedPopupCount();
  for (size_t i = 0; i < popup_count; ++i) {
    string16 url, title;
    container->GetURLAndTitleForPopup(i, &url, &title);
    // We can't just use the index into container_ here because Menu reserves
    // the value 0 as the nop command.
    container->launch_menu_->AppendMenuItemWithLabel(i + 1,
        l10n_util::GetStringFUTF8(IDS_POPUP_TITLE_FORMAT, url, title));
  }

  // Set items (kImpossibleNumberOfPopups + 1) ..
  // (kImpossibleNumberOfPopups + 1 + hosts.size()) as hosts.
  std::vector<std::string> hosts(container->model_->GetHosts());
  if (!hosts.empty() && (popup_count > 0))
    container->launch_menu_->AppendSeparator();
  for (size_t i = 0; i < hosts.size(); ++i) {
    container->launch_menu_->AppendCheckMenuItemWithLabel(
        BlockedPopupContainer::kImpossibleNumberOfPopups + i + 1,
        l10n_util::GetStringFUTF8(IDS_POPUP_HOST_FORMAT,
                                  UTF8ToUTF16(hosts[i])));
  }

  container->launch_menu_->PopupAsContext(gtk_get_current_event_time());
}

void BlockedPopupContainerViewGtk::OnCloseButtonClicked(
    GtkButton *button, BlockedPopupContainerViewGtk* container) {
  container->model_->set_dismissed();
  container->model_->CloseAll();
}

gboolean BlockedPopupContainerViewGtk::OnContainerExpose(
    GtkWidget* widget, GdkEventExpose* event,
    BlockedPopupContainerViewGtk* container) {
  int width = widget->allocation.width;
  int height = widget->allocation.height;

  // Update our window shape if we need to.
  if (container->notification_width_ != widget->allocation.width ||
      container->notification_height_ != widget->allocation.height) {
    // We need to update the shape of the status bubble whenever our GDK
    // window changes shape.
    std::vector<GdkPoint> mask_points = MakeFramePolygonPoints(
        widget->allocation.width, widget->allocation.height, FRAME_MASK);
    GdkRegion* mask_region = gdk_region_polygon(&mask_points[0],
                                                mask_points.size(),
                                                GDK_EVEN_ODD_RULE);
    gdk_window_shape_combine_region(widget->window, mask_region, 0, 0);
    gdk_region_destroy(mask_region);

    container->notification_width_ = widget->allocation.width;
    container->notification_height_ = widget->allocation.height;
  }

  if (!container->theme_provider_->UseGtkTheme()) {
    // Clip to our damage rect.
    cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(event->window));
    cairo_rectangle(cr, event->area.x, event->area.y,
                    event->area.width, event->area.height);
    cairo_clip(cr);

    // TODO(erg): We draw the gradient background only when GTK themes are
    // off. This isn't a perfect solution as this isn't themed! The views
    // version doesn't appear to be themed either, so at least for now,
    // constants are OK.
    int half_width = width / 2;
    cairo_pattern_t* pattern = cairo_pattern_create_linear(
        half_width, 0,  half_width, height);
    cairo_pattern_add_color_stop_rgb(
        pattern, 0.0,
        kBackgroundColorTop[0], kBackgroundColorTop[1], kBackgroundColorTop[2]);
    cairo_pattern_add_color_stop_rgb(
        pattern, 1.0,
        kBackgroundColorBottom[0], kBackgroundColorBottom[1],
        kBackgroundColorBottom[2]);
    cairo_set_source(cr, pattern);
    cairo_paint(cr);
    cairo_pattern_destroy(pattern);

    cairo_destroy(cr);
  }

  GdkDrawable* drawable = GDK_DRAWABLE(event->window);
  GdkGC* gc = gdk_gc_new(drawable);
  if (container->theme_provider_->UseGtkTheme()) {
    GdkColor color = container->theme_provider_->GetBorderColor();
    gdk_gc_set_rgb_fg_color(gc, &color);
  } else {
    gdk_gc_set_rgb_fg_color(gc, &kBorderColor);
  }

  // Stroke the frame border.
  std::vector<GdkPoint> points = MakeFramePolygonPoints(
      widget->allocation.width, widget->allocation.height, FRAME_STROKE);
  gdk_draw_lines(drawable, gc, &points[0], points.size());

  g_object_unref(gc);
  return FALSE;  // Allow subwidgets to paint.
}
