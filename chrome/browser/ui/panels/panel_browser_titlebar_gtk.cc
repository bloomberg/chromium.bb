// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_titlebar_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_window_gtk.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/skia_utils_gtk.h"

namespace {

// Padding around the titlebar.
const int kPanelTitlebarPaddingTop = 7;
const int kPanelTitlebarPaddingBottom = 7;
const int kPanelTitlebarPaddingLeft = 4;
const int kPanelTitlebarPaddingRight = 9;

// Spacing between buttons of panel's titlebar.
const int kPanelButtonSpacing = 9;

// Spacing between the icon and the title text.
const int kPanelIconTitleSpacing = 9;

// Color used to draw title text under default theme.
const SkColor kTitleTextDefaultColor = SkColorSetRGB(0xf9, 0xf9, 0xf9);

// Markup used to paint the title with the desired font.
const char* const kTitleMarkupPrefix = "<span face='Arial' size='11264'>";
const char* const kTitleMarkupSuffix = "</span>";

}  // namespace

PanelBrowserTitlebarGtk::PanelBrowserTitlebarGtk(
    PanelBrowserWindowGtk* browser_window, GtkWindow* window)
    : browser_window_(browser_window),
      window_(window),
      container_(NULL),
      titlebar_right_buttons_vbox_(NULL),
      titlebar_right_buttons_hbox_(NULL),
      icon_(NULL),
      title_(NULL),
      theme_service_(NULL) {
}

PanelBrowserTitlebarGtk::~PanelBrowserTitlebarGtk() {
}

void PanelBrowserTitlebarGtk::Init() {
  container_ = gtk_event_box_new();
  gtk_widget_set_name(container_, "chrome-panel-titlebar");
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(container_), FALSE);

  // We use an alignment to control the titlebar paddings.
  GtkWidget* container_alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_container_add(GTK_CONTAINER(container_), container_alignment);
  gtk_alignment_set_padding(GTK_ALIGNMENT(container_alignment),
                            kPanelTitlebarPaddingTop,
                            kPanelTitlebarPaddingBottom,
                            kPanelTitlebarPaddingLeft,
                            kPanelTitlebarPaddingRight);

  // Add a container box.
  GtkWidget* container_hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(container_alignment), container_hbox);

  g_signal_connect(window_, "window-state-event",
                   G_CALLBACK(OnWindowStateChangedThunk), this);

  // Add minimize/restore and close buttons. Panel buttons are always placed
  // on the right part of the titlebar.
  titlebar_right_buttons_vbox_ = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_end(GTK_BOX(container_hbox), titlebar_right_buttons_vbox_,
                   FALSE, FALSE, 0);
  BuildButtons();

  // Add hbox for holding icon and title.
  GtkWidget* icon_title_hbox = gtk_hbox_new(FALSE, kPanelIconTitleSpacing);
  gtk_box_pack_start(GTK_BOX(container_hbox), icon_title_hbox, TRUE, TRUE, 0);

  // Add icon. We use the app logo as a placeholder image so the title doesn't
  // jump around.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon_ = gtk_image_new_from_pixbuf(rb.GetNativeImageNamed(
      IDR_PRODUCT_LOGO_16, ui::ResourceBundle::RTL_ENABLED).ToGdkPixbuf());
  g_object_set_data(G_OBJECT(icon_), "left-align-popup",
                    reinterpret_cast<void*>(true));
  gtk_box_pack_start(GTK_BOX(icon_title_hbox), icon_, FALSE, FALSE, 0);

  // Add title.
  title_ = gtk_label_new(NULL);
  gtk_label_set_ellipsize(GTK_LABEL(title_), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment(GTK_MISC(title_), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(icon_title_hbox), title_, TRUE, TRUE, 0);

  UpdateTitleAndIcon();

  theme_service_ = GtkThemeService::GetFrom(
      browser_window_->panel()->profile());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
  theme_service_->InitThemesFor(this);

  gtk_widget_show_all(container_);
}

SkColor PanelBrowserTitlebarGtk::GetTextColor() const {
  if (browser_window_->UsingDefaultTheme())
    return kTitleTextDefaultColor;
  return theme_service_->GetColor(browser_window_->paint_state() ==
      PanelBrowserWindowGtk::PAINT_AS_ACTIVE ?
          ThemeService::COLOR_TAB_TEXT :
          ThemeService::COLOR_BACKGROUND_TAB_TEXT);
}

void PanelBrowserTitlebarGtk::BuildButtons() {
  minimize_button_.reset(CreateButton(panel::MINIMIZE_BUTTON));
  restore_button_.reset(CreateButton(panel::RESTORE_BUTTON));
  close_button_.reset(CreateButton(panel::CLOSE_BUTTON));

  // We control visibility of minimize and restore buttons.
  gtk_widget_set_no_show_all(minimize_button_->widget(), TRUE);
  gtk_widget_set_no_show_all(restore_button_->widget(), TRUE);

  // Now show the correct widgets in the two hierarchies.
  UpdateMinimizeRestoreButtonVisibility();
}

CustomDrawButton* PanelBrowserTitlebarGtk::CreateButton(
    panel::TitlebarButtonType button_type) {
  int normal_image_id = -1;
  int pressed_image_id = -1;
  int hover_image_id = -1;
  int tooltip_id = -1;
  GetButtonResources(button_type, &normal_image_id, &pressed_image_id,
                     &hover_image_id, &tooltip_id);

  CustomDrawButton* button = new CustomDrawButton(normal_image_id,
                                                  pressed_image_id,
                                                  hover_image_id,
                                                  0);
  gtk_widget_add_events(GTK_WIDGET(button->widget()), GDK_POINTER_MOTION_MASK);
  g_signal_connect(button->widget(), "clicked",
                   G_CALLBACK(OnButtonClickedThunk), this);

  std::string localized_tooltip = l10n_util::GetStringUTF8(tooltip_id);
  gtk_widget_set_tooltip_text(button->widget(),
                              localized_tooltip.c_str());

  GtkWidget* box = GetButtonHBox();
  gtk_box_pack_start(GTK_BOX(box), button->widget(), FALSE, FALSE, 0);
  return button;
}

void PanelBrowserTitlebarGtk::GetButtonResources(
    panel::TitlebarButtonType button_type,
    int* normal_image_id,
    int* pressed_image_id,
    int* hover_image_id,
    int* tooltip_id) const {
  switch (button_type) {
    case panel::CLOSE_BUTTON:
      *normal_image_id = IDR_PANEL_CLOSE;
      *pressed_image_id = IDR_PANEL_CLOSE_C;
      *hover_image_id = IDR_PANEL_CLOSE_H;
      *tooltip_id = IDS_PANEL_CLOSE_TOOLTIP;
      break;
    case panel::MINIMIZE_BUTTON:
      *normal_image_id = IDR_PANEL_MINIMIZE;
      *pressed_image_id = IDR_PANEL_MINIMIZE_C;
      *hover_image_id = IDR_PANEL_MINIMIZE_H;
      *tooltip_id = IDS_PANEL_MINIMIZE_TOOLTIP;
      break;
    case panel::RESTORE_BUTTON:
      *normal_image_id = IDR_PANEL_RESTORE;
      *pressed_image_id = IDR_PANEL_RESTORE_C;
      *hover_image_id = IDR_PANEL_RESTORE_H;
      *tooltip_id = IDS_PANEL_RESTORE_TOOLTIP;
      break;
  }
}

GtkWidget* PanelBrowserTitlebarGtk::GetButtonHBox() {
  if (!titlebar_right_buttons_hbox_) {
    // We put the minimize/restore/close buttons in a vbox so they are top
    // aligned (up to padding) and don't vertically stretch.
    titlebar_right_buttons_hbox_ = gtk_hbox_new(FALSE, kPanelButtonSpacing);
    gtk_box_pack_start(GTK_BOX(titlebar_right_buttons_vbox_),
                       titlebar_right_buttons_hbox_, FALSE, FALSE, 0);
  }

  return titlebar_right_buttons_hbox_;
}

void PanelBrowserTitlebarGtk::UpdateCustomFrame(bool use_custom_frame) {
  // Nothing to do.
}

void PanelBrowserTitlebarGtk::UpdateTitleAndIcon() {
  std::string title_text =
      UTF16ToUTF8(browser_window_->panel()->GetWindowTitle());

  // Add the markup to show the title in the desired font.
  gchar* escaped_title_text = g_markup_escape_text(title_text.c_str(), -1);
  gchar* title_text_with_markup = g_strconcat(kTitleMarkupPrefix,
                                              escaped_title_text,
                                              kTitleMarkupSuffix,
                                              NULL);
  gtk_label_set_markup(GTK_LABEL(title_), title_text_with_markup);
  g_free(escaped_title_text);
  g_free(title_text_with_markup);
}

void PanelBrowserTitlebarGtk::UpdateThrobber(
    content::WebContents* web_contents) {
  if (web_contents && web_contents->IsLoading()) {
    GdkPixbuf* icon_pixbuf =
        throbber_.GetNextFrame(web_contents->IsWaitingForResponse());
    gtk_image_set_from_pixbuf(GTK_IMAGE(icon_), icon_pixbuf);
  } else {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    SkBitmap icon = browser_window_->panel()->GetCurrentPageIcon();
    if (icon.empty()) {
      // Fallback to the Chromium icon if the page has no icon.
      gtk_image_set_from_pixbuf(GTK_IMAGE(icon_),
          rb.GetNativeImageNamed(IDR_PRODUCT_LOGO_16).ToGdkPixbuf());
    } else {
      GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(icon);
      gtk_image_set_from_pixbuf(GTK_IMAGE(icon_), icon_pixbuf);
      g_object_unref(icon_pixbuf);
    }

    throbber_.Reset();
  }
}

void PanelBrowserTitlebarGtk::ShowContextMenu(GdkEventButton* event) {
  // Panel does not show any context menu.
}

void PanelBrowserTitlebarGtk::UpdateTextColor() {
  GdkColor text_color = gfx::SkColorToGdkColor(GetTextColor());
  gtk_util::SetLabelColor(title_, &text_color);
}

void PanelBrowserTitlebarGtk::UpdateMinimizeRestoreButtonVisibility() {
  Panel* panel = browser_window_->panel();
  gtk_widget_set_visible(minimize_button_->widget(), panel->CanMinimize());
  gtk_widget_set_visible(restore_button_->widget(), panel->CanRestore());
}

gboolean PanelBrowserTitlebarGtk::OnWindowStateChanged(
    GtkWindow* window, GdkEventWindowState* event) {
  UpdateTextColor();
  return FALSE;
}

void PanelBrowserTitlebarGtk::OnButtonClicked(GtkWidget* button) {
  if (close_button_->widget() == button) {
    browser_window_->panel()->Close();
    return;
  }

  GdkEvent* event = gtk_get_current_event();
  DCHECK(event && event->type == GDK_BUTTON_RELEASE);

  if (minimize_button_->widget() == button) {
    browser_window_->panel()->OnMinimizeButtonClicked(
        (event->button.state & GDK_CONTROL_MASK) ?
            panel::APPLY_TO_ALL : panel::NO_MODIFIER);
  } else if (restore_button_->widget() == button) {
    browser_window_->panel()->OnRestoreButtonClicked(
        (event->button.state & GDK_CONTROL_MASK) ?
            panel::APPLY_TO_ALL : panel::NO_MODIFIER);
  }

  gdk_event_free(event);
}

void PanelBrowserTitlebarGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      UpdateTextColor();
      break;
    default:
      NOTREACHED();
  }
}

void PanelBrowserTitlebarGtk::SendEnterNotifyToCloseButtonIfUnderMouse() {
  if (!close_button())
    return;

  gint x;
  gint y;
  GtkAllocation widget_allocation = close_button()->WidgetAllocation();
  gtk_widget_get_pointer(GTK_WIDGET(close_button()->widget()), &x, &y);

  gfx::Rect button_rect(0, 0, widget_allocation.width,
                        widget_allocation.height);
  if (!button_rect.Contains(x, y)) {
    // Mouse is not over the close button.
    return;
  }

  // Create and emit an enter-notify-event on close button.
  GValue return_value;
  return_value.g_type = G_TYPE_BOOLEAN;
  g_value_set_boolean(&return_value, false);

  GdkEvent* event = gdk_event_new(GDK_ENTER_NOTIFY);
  event->crossing.window =
      gtk_button_get_event_window(GTK_BUTTON(close_button()->widget()));
  event->crossing.send_event = FALSE;
  event->crossing.subwindow = gtk_widget_get_window(close_button()->widget());
  event->crossing.time = gtk_util::XTimeNow();
  event->crossing.x = x;
  event->crossing.y = y;
  event->crossing.x_root = widget_allocation.x;
  event->crossing.y_root = widget_allocation.y;
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
  event->crossing.focus = true;
  event->crossing.state = 0;

  g_signal_emit_by_name(GTK_OBJECT(close_button()->widget()),
                        "enter-notify-event", event,
                        &return_value);
}

GtkWidget* PanelBrowserTitlebarGtk::widget() const {
  return container_;
}

void PanelBrowserTitlebarGtk::set_window(GtkWindow* window) {
  window_ = window;
}

AvatarMenuButtonGtk* PanelBrowserTitlebarGtk::avatar_button() const {
  // Not supported in panel.
  return NULL;
}
