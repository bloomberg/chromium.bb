// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/action_box_button_gtk.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "chrome/browser/extensions/extension_icon_image.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

using extensions::ActionInfo;
using extensions::Extension;
using extensions::IconImage;

namespace {

// This class provides a GtkWidget that shows an Extension's page launcher icon.
// We can't use GtkImage directly because loading the icon from the extension
// must be done asynchronously. The lifetime of this object is tied to its
// GtkWidget, and it will delete itself upon receiving a "destroy" signal from
// the widget.
class ExtensionIcon : public IconImage::Observer {
 public:
  ExtensionIcon(Profile* profile, const Extension* extension);

  GtkWidget* GetWidget();

 private:
  virtual ~ExtensionIcon() {}

  virtual void OnExtensionIconImageChanged(IconImage* image) OVERRIDE;
  void UpdateIcon();

  CHROMEGTK_CALLBACK_0(ExtensionIcon, void, OnDestroyed);

  GtkWidget* image_;

  scoped_ptr<IconImage> icon_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionIcon);
};

ExtensionIcon::ExtensionIcon(Profile* profile, const Extension* extension)
    : image_(NULL) {
  const ActionInfo* page_launcher_info =
      ActionInfo::GetPageLauncherInfo(extension);
  icon_.reset(new IconImage(profile,
                            extension,
                            page_launcher_info->default_icon,
                            extension_misc::EXTENSION_ICON_ACTION,
                            extensions::IconsInfo::GetDefaultAppIcon(),
                            this));
  UpdateIcon();
}

GtkWidget* ExtensionIcon::GetWidget() {
  return image_;
}

void ExtensionIcon::OnExtensionIconImageChanged(IconImage* image)  {
  DCHECK_EQ(image, icon_.get());
  UpdateIcon();
}

void ExtensionIcon::UpdateIcon() {
  const gfx::ImageSkiaRep& rep =
      icon_->image_skia().GetRepresentation(ui::SCALE_FACTOR_NONE);
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(rep.sk_bitmap());

  if (!image_) {
    image_ = gtk_image_new_from_pixbuf(pixbuf);
    g_signal_connect(image_, "destroy", G_CALLBACK(OnDestroyedThunk), this);
  } else {
    gtk_image_set_from_pixbuf(GTK_IMAGE(image_), pixbuf);
  }
  g_object_unref(pixbuf);
}

void ExtensionIcon::OnDestroyed(GtkWidget* sender) {
  DCHECK_EQ(sender, image_);
  delete this;
}

}  // namespace

ActionBoxButtonGtk::ActionBoxButtonGtk(Browser* browser)
    : controller_(browser, this),
      browser_(browser) {
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

GtkWidget* ActionBoxButtonGtk::GetImageForCommandId(int command_id) const {
  int index = model_->GetIndexOfCommandId(command_id);
  if (model_->IsItemExtension(index)) {
    const Extension* extension = model_->GetExtensionAt(index);
    return (new ExtensionIcon(browser_->profile(), extension))->GetWidget();
  }

  return GetDefaultImageForCommandId(command_id);
}

GtkWidget* ActionBoxButtonGtk::widget() {
  return button_->widget();
}

void ActionBoxButtonGtk::ShowMenu(scoped_ptr<ActionBoxMenuModel> model) {
  model_ = model.Pass();
  menu_.reset(new MenuGtk(this, model_.get()));
  menu_->PopupForWidget(
      button_->widget(),
      // The mouse button. This is 1 because currently it can only be generated
      // from a mouse click, but if ShowMenu can be called in other situations
      // (e.g. other mouse buttons, or without any click at all) then it will
      // need to be that button or 0.
      1,
      gtk_get_current_event_time());
}

gboolean ActionBoxButtonGtk::OnButtonPress(GtkWidget* widget,
                                           GdkEventButton* event) {
  if (event->button == 1)
    controller_.OnButtonClicked();
  return FALSE;
}
