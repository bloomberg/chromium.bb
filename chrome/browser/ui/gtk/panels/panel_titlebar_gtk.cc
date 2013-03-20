// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/panels/panel_titlebar_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/panels/panel_gtk.h"
#include "chrome/browser/ui/panels/panel.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_utils_gtk.h"

namespace {

// Padding around the titlebar.
const int kPanelTitlebarPaddingTop = 4;
const int kPanelTitlebarPaddingBottom = 8;
const int kPanelTitlebarPaddingLeft = 6;
const int kPanelTitlebarPaddingRight = 0;

// Padding around the box containing icon and title.
const int kPanelIconTitlePaddingTop = 3;
const int kPanelIconTitlePaddingBottom = 0;
const int kPanelIconTitlePaddingLeft = 0;
const int kPanelIconTitlePaddingRight = 0;

// Spacing between buttons of panel's titlebar.
const int kPanelButtonSpacing = 5;

// Spacing between the icon and the title text.
const int kPanelIconTitleSpacing = 9;

// Color used to draw title text under default theme.
const SkColor kTitleTextDefaultColor = SkColorSetRGB(0xf9, 0xf9, 0xf9);

// Markup used to paint the title with the desired font.
const char* const kTitleMarkupPrefix =
    "<span face='Arial' weight='bold' size='11264'>";
const char* const kTitleMarkupSuffix = "</span>";

}  // namespace

PanelTitlebarGtk::PanelTitlebarGtk(PanelGtk* panel_gtk)
    : panel_gtk_(panel_gtk),
      container_(NULL),
      titlebar_right_buttons_vbox_(NULL),
      titlebar_right_buttons_hbox_(NULL),
      icon_(NULL),
      title_(NULL) {
}

PanelTitlebarGtk::~PanelTitlebarGtk() {
}

void PanelTitlebarGtk::Init() {
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

  // Add minimize/restore and close buttons. Panel buttons are always placed
  // on the right part of the titlebar.
  titlebar_right_buttons_vbox_ = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_end(GTK_BOX(container_hbox), titlebar_right_buttons_vbox_,
                   FALSE, FALSE, 0);
  BuildButtons();

  // Add an extra alignment to control the paddings for icon and title.
  GtkWidget* icon_title_alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_container_add(GTK_CONTAINER(container_hbox), icon_title_alignment);
  gtk_alignment_set_padding(GTK_ALIGNMENT(icon_title_alignment),
                            kPanelIconTitlePaddingTop,
                            kPanelIconTitlePaddingBottom,
                            kPanelIconTitlePaddingLeft,
                            kPanelIconTitlePaddingRight);

  // Add hbox for holding icon and title.
  GtkWidget* icon_title_hbox = gtk_hbox_new(FALSE, kPanelIconTitleSpacing);
  gtk_container_add(GTK_CONTAINER(icon_title_alignment), icon_title_hbox);

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
  UpdateTextColor();

  gtk_widget_show_all(container_);
}

SkColor PanelTitlebarGtk::GetTextColor() const {
  return kTitleTextDefaultColor;
}

void PanelTitlebarGtk::BuildButtons() {
  minimize_button_.reset(CreateButton(panel::MINIMIZE_BUTTON));
  restore_button_.reset(CreateButton(panel::RESTORE_BUTTON));
  close_button_.reset(CreateButton(panel::CLOSE_BUTTON));

  // We control visibility of minimize and restore buttons.
  gtk_widget_set_no_show_all(minimize_button_->widget(), TRUE);
  gtk_widget_set_no_show_all(restore_button_->widget(), TRUE);

  // Now show the correct widgets in the two hierarchies.
  UpdateMinimizeRestoreButtonVisibility();
}

CustomDrawButton* PanelTitlebarGtk::CreateButton(
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
  gtk_widget_set_size_request(button->widget(),
                              panel::kPanelButtonSize,
                              panel::kPanelButtonSize);
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

void PanelTitlebarGtk::GetButtonResources(
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

GtkWidget* PanelTitlebarGtk::GetButtonHBox() {
  if (!titlebar_right_buttons_hbox_) {
    // We put the minimize/restore/close buttons in a vbox so they are top
    // aligned (up to padding) and don't vertically stretch.
    titlebar_right_buttons_hbox_ = gtk_hbox_new(FALSE, kPanelButtonSpacing);
    gtk_box_pack_start(GTK_BOX(titlebar_right_buttons_vbox_),
                       titlebar_right_buttons_hbox_, FALSE, FALSE, 0);
  }

  return titlebar_right_buttons_hbox_;
}

void PanelTitlebarGtk::UpdateTitleAndIcon() {
  std::string title_text =
      UTF16ToUTF8(panel_gtk_->panel()->GetWindowTitle());

  // Add the markup to show the title in the desired font.
  gchar* escaped_title_text = g_markup_escape_text(title_text.c_str(), -1);
  gchar* title_text_with_markup = g_strconcat(kTitleMarkupPrefix,
                                              escaped_title_text,
                                              kTitleMarkupSuffix,
                                              NULL);
  gtk_label_set_markup(GTK_LABEL(title_), title_text_with_markup);
  g_free(escaped_title_text);
  g_free(title_text_with_markup);

  // Update icon from the web contents.
  content::WebContents* web_contents = panel_gtk_->panel()->GetWebContents();
  if (web_contents)
    UpdateThrobber(web_contents);
}

void PanelTitlebarGtk::UpdateThrobber(
    content::WebContents* web_contents) {
  if (web_contents && web_contents->IsLoading()) {
    GdkPixbuf* icon_pixbuf =
        throbber_.GetNextFrame(web_contents->IsWaitingForResponse());
    gtk_image_set_from_pixbuf(GTK_IMAGE(icon_), icon_pixbuf);
  } else {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    gfx::Image icon = panel_gtk_->panel()->GetCurrentPageIcon();
    if (icon.IsEmpty()) {
      // Fallback to the Chromium icon if the page has no icon.
      gtk_image_set_from_pixbuf(GTK_IMAGE(icon_),
          rb.GetNativeImageNamed(IDR_PRODUCT_LOGO_16).ToGdkPixbuf());
    } else {
      gtk_image_set_from_pixbuf(GTK_IMAGE(icon_), icon.ToGdkPixbuf());
    }

    throbber_.Reset();
  }
}

void PanelTitlebarGtk::UpdateTextColor() {
  GdkColor text_color = gfx::SkColorToGdkColor(GetTextColor());
  gtk_util::SetLabelColor(title_, &text_color);
}

void PanelTitlebarGtk::UpdateMinimizeRestoreButtonVisibility() {
  Panel* panel = panel_gtk_->panel();
  gtk_widget_set_visible(minimize_button_->widget(),
                         panel->CanShowMinimizeButton());
  gtk_widget_set_visible(restore_button_->widget(),
                         panel->CanShowRestoreButton());
}

void PanelTitlebarGtk::OnButtonClicked(GtkWidget* button) {
  Panel* panel = panel_gtk_->panel();
  if (close_button_->widget() == button) {
    panel->Close();
    return;
  }

  GdkEvent* event = gtk_get_current_event();
  DCHECK(event && event->type == GDK_BUTTON_RELEASE);

  if (minimize_button_->widget() == button) {
    panel->OnMinimizeButtonClicked(
        (event->button.state & GDK_CONTROL_MASK) ?
            panel::APPLY_TO_ALL : panel::NO_MODIFIER);
  } else if (restore_button_->widget() == button) {
    panel->OnRestoreButtonClicked(
        (event->button.state & GDK_CONTROL_MASK) ?
            panel::APPLY_TO_ALL : panel::NO_MODIFIER);
    panel->Activate();
  }

  gdk_event_free(event);
}

void PanelTitlebarGtk::SendEnterNotifyToCloseButtonIfUnderMouse() {
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

GtkWidget* PanelTitlebarGtk::widget() const {
  return container_;
}
