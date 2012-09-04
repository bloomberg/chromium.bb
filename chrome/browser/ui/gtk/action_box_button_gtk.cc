// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/action_box_button_gtk.h"

#include <gtk/gtk.h>

#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

ActionBoxButtonGtk::ActionBoxButtonGtk(Browser* browser) : browser_(browser) {
  button_.reset(new CustomDrawButton(
      IDR_ACTION_BOX_BUTTON,
      IDR_ACTION_BOX_BUTTON_PRESSED,
      IDR_ACTION_BOX_BUTTON_PRESSED,  // TODO: hover
      0));  // TODO: disabled?
  gtk_widget_set_tooltip_text(widget(),
      l10n_util::GetStringUTF8(IDS_TOOLTIP_ACTION_BOX_BUTTON).c_str());

  g_signal_connect(widget(), "button-press-event",
                   G_CALLBACK(OnButtonPressThunk), this);

  ViewIDUtil::SetID(widget(), VIEW_ID_ACTION_BOX_BUTTON);
}

ActionBoxButtonGtk::~ActionBoxButtonGtk() {
}

bool ActionBoxButtonGtk::AlwaysShowIconForCmd(int command_id) const {
  return true;
}

GtkWidget* ActionBoxButtonGtk::widget() {
  return button_->widget();
}

gboolean ActionBoxButtonGtk::OnButtonPress(GtkWidget* widget,
                                           GdkEventButton* event) {
  if (event->button != 1)
    return FALSE;

  model_.reset(new ActionBoxMenuModel(browser_));
  menu_.reset(new MenuGtk(this, model_.get()));
  menu_->PopupForWidget(button_->widget(), event->button, event->time);

  return FALSE;
}
