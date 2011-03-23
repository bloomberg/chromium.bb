// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/back_forward_button_gtk.h"

#include <gtk/gtk.h>

#include "base/message_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

// The time in milliseconds between when the user clicks and the menu appears.
static const int kMenuTimerDelay = 500;

BackForwardButtonGtk::BackForwardButtonGtk(Browser* browser, bool is_forward)
    : browser_(browser),
      is_forward_(is_forward),
      show_menu_factory_(this) {
  int normal, pushed, hover, disabled, tooltip;
  const char* stock;
  if (is_forward) {
    normal = IDR_FORWARD;
    pushed = IDR_FORWARD_P;
    hover = IDR_FORWARD_H;
    disabled = IDR_FORWARD_D;
    tooltip = IDS_TOOLTIP_FORWARD;
    stock = GTK_STOCK_GO_FORWARD;
  } else {
    normal = IDR_BACK;
    pushed = IDR_BACK_P;
    hover = IDR_BACK_H;
    disabled = IDR_BACK_D;
    tooltip = IDS_TOOLTIP_BACK;
    stock = GTK_STOCK_GO_BACK;
  }
  button_.reset(new CustomDrawButton(
      GtkThemeService::GetFrom(browser_->profile()),
      normal, pushed, hover, disabled, stock, GTK_ICON_SIZE_SMALL_TOOLBAR));
  gtk_widget_set_tooltip_text(widget(),
                              l10n_util::GetStringUTF8(tooltip).c_str());
  menu_model_.reset(new BackForwardMenuModel(browser, is_forward ?
      BackForwardMenuModel::FORWARD_MENU :
      BackForwardMenuModel::BACKWARD_MENU));

  g_signal_connect(widget(), "clicked",
                   G_CALLBACK(OnClickThunk), this);
  g_signal_connect(widget(), "button-press-event",
                   G_CALLBACK(OnButtonPressThunk), this);
  gtk_widget_add_events(widget(), GDK_POINTER_MOTION_MASK);
  g_signal_connect(widget(), "motion-notify-event",
                   G_CALLBACK(OnMouseMoveThunk), this);

  // Popup the menu as left-aligned relative to this widget rather than the
  // default of right aligned.
  g_object_set_data(G_OBJECT(widget()), "left-align-popup",
                    reinterpret_cast<void*>(true));

  gtk_util::SetButtonTriggersNavigation(widget());
}

BackForwardButtonGtk::~BackForwardButtonGtk() {
}

void BackForwardButtonGtk::StoppedShowing() {
  button_->UnsetPaintOverride();
}

bool BackForwardButtonGtk::AlwaysShowIconForCmd(int command_id) const {
  return true;
}

void BackForwardButtonGtk::ShowBackForwardMenu(int button, guint32 event_time) {
  menu_.reset(new MenuGtk(this, menu_model_.get()));
  button_->SetPaintOverride(GTK_STATE_ACTIVE);
  menu_->PopupForWidget(widget(), button, event_time);
}

void BackForwardButtonGtk::OnClick(GtkWidget* widget) {
  show_menu_factory_.RevokeAll();

  browser_->ExecuteCommandWithDisposition(
      is_forward_ ? IDC_FORWARD : IDC_BACK,
      gtk_util::DispositionForCurrentButtonPressEvent());
}

gboolean BackForwardButtonGtk::OnButtonPress(GtkWidget* widget,
                                             GdkEventButton* event) {
  if (event->button == 3)
    ShowBackForwardMenu(event->button, event->time);

  if (event->button != 1)
    return FALSE;

  y_position_of_last_press_ = static_cast<int>(event->y);
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      show_menu_factory_.NewRunnableMethod(
          &BackForwardButtonGtk::ShowBackForwardMenu,
          event->button, event->time),
      kMenuTimerDelay);
  return FALSE;
}

gboolean BackForwardButtonGtk::OnMouseMove(GtkWidget* widget,
                                           GdkEventMotion* event) {
  // If we aren't waiting to show the back forward menu, do nothing.
  if (show_menu_factory_.empty())
    return FALSE;

  // We only count moves about a certain threshold.
  GtkSettings* settings = gtk_widget_get_settings(widget);
  int drag_min_distance;
  g_object_get(settings, "gtk-dnd-drag-threshold", &drag_min_distance, NULL);
  if (event->y - y_position_of_last_press_ < drag_min_distance)
    return FALSE;

  // We will show the menu now. Cancel the delayed event.
  show_menu_factory_.RevokeAll();
  ShowBackForwardMenu(/* button */ 1, event->time);
  return FALSE;
}
