// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/extension_infobar_gtk.h"

#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/infobars/infobar_container_gtk.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

ExtensionInfoBarGtk::ExtensionInfoBarGtk(InfoBarTabHelper* owner,
                                         ExtensionInfoBarDelegate* delegate)
    : InfoBarGtk(owner, delegate),
      tracker_(this),
      delegate_(delegate),
      view_(NULL) {
  // Always render the close button as if we were doing chrome style widget
  // rendering. For extension infobars, we force chrome style rendering because
  // extension authors are going to expect to match the declared gradient in
  // extensions_infobar.css, and the close button provided by some GTK+ themes
  // won't look good on this background.
  close_button_->ForceChromeTheme();

  int height = delegate->height();
  SetBarTargetHeight((height > 0) ? (height + kSeparatorLineHeight) : 0);

  BuildWidgets();
}

ExtensionInfoBarGtk::~ExtensionInfoBarGtk() {}

void ExtensionInfoBarGtk::PlatformSpecificHide(bool animate) {
  // This view is not owned by us; we can't unparent it because we aren't the
  // owning container.
  gtk_container_remove(GTK_CONTAINER(alignment_), view_->native_view());
}

void ExtensionInfoBarGtk::GetTopColor(InfoBarDelegate::Type type,
                                      double* r, double* g, double* b) {
  // Extension infobars are always drawn with chrome-theme colors.
  *r = *g = *b = 233.0 / 255.0;
}

void ExtensionInfoBarGtk::GetBottomColor(InfoBarDelegate::Type type,
                                         double* r, double* g, double* b) {
  *r = *g = *b = 218.0 / 255.0;
}

void ExtensionInfoBarGtk::OnImageLoaded(
    SkBitmap* image, const ExtensionResource& resource, int index) {
  if (!delegate_)
    return;  // The delegate can go away while we asynchronously load images.

  // TODO(erg): IDR_EXTENSIONS_SECTION should have an IDR_INFOBAR_EXTENSIONS
  // icon of the correct size with real subpixel shading and such.
  SkBitmap* icon = image;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (!image || image->empty())
    icon = rb.GetBitmapNamed(IDR_EXTENSIONS_SECTION);

  SkBitmap* drop_image = rb.GetBitmapNamed(IDR_APP_DROPARROW);

  int image_size = Extension::EXTENSION_ICON_BITTY;
  // The margin between the extension icon and the drop-down arrow bitmap.
  static const int kDropArrowLeftMargin = 3;
  scoped_ptr<gfx::CanvasSkia> canvas(new gfx::CanvasSkia(
      image_size + kDropArrowLeftMargin + drop_image->width(),
      image_size,
      false));
  canvas->DrawBitmapInt(*icon, 0, 0, icon->width(), icon->height(), 0, 0,
                        image_size, image_size, false);
  canvas->DrawBitmapInt(*drop_image, image_size + kDropArrowLeftMargin,
                        image_size / 2);

  SkBitmap bitmap = canvas->ExtractBitmap();
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&bitmap);
  gtk_image_set_from_pixbuf(GTK_IMAGE(icon_), pixbuf);
  g_object_unref(pixbuf);
}

void ExtensionInfoBarGtk::BuildWidgets() {
  button_ = gtk_chrome_button_new();
  gtk_chrome_button_set_use_gtk_rendering(GTK_CHROME_BUTTON(button_), FALSE);
  g_object_set_data(G_OBJECT(button_), "left-align-popup",
                    reinterpret_cast<void*>(true));

  icon_ = gtk_image_new();
  gtk_misc_set_alignment(GTK_MISC(icon_), 0.5, 0.5);
  gtk_button_set_image(GTK_BUTTON(button_), icon_);
  gtk_util::CenterWidgetInHBox(hbox_, button_, false, 0);

  // Start loading the image for the menu button.
  const Extension* extension = delegate_->extension_host()->extension();
  ExtensionResource icon_resource = extension->GetIconResource(
      Extension::EXTENSION_ICON_BITTY, ExtensionIconSet::MATCH_EXACTLY);
  if (!icon_resource.relative_path().empty()) {
    // Create a tracker to load the image. It will report back on OnImageLoaded.
    tracker_.LoadImage(extension, icon_resource,
                       gfx::Size(Extension::EXTENSION_ICON_BITTY,
                                 Extension::EXTENSION_ICON_BITTY),
                       ImageLoadingTracker::DONT_CACHE);
  } else {
    OnImageLoaded(NULL, icon_resource, 0);
  }

  // Pad the bottom of the infobar by one pixel for the border.
  alignment_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment_), 0, 1, 0, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), alignment_, TRUE, TRUE, 0);

  ExtensionHost* extension_host = delegate_->extension_host();
  view_ = extension_host->view();

  if (gtk_widget_get_parent(view_->native_view())) {
    gtk_widget_reparent(view_->native_view(), alignment_);
  } else {
    gtk_container_add(GTK_CONTAINER(alignment_), view_->native_view());
  }

  Signals()->Connect(button_, "button-press-event",
                     G_CALLBACK(&OnButtonPressThunk), this);
  Signals()->Connect(view_->native_view(), "expose-event",
                     G_CALLBACK(&OnExposeThunk), this);
  Signals()->Connect(view_->native_view(), "size_allocate",
                     G_CALLBACK(&OnSizeAllocateThunk), this);
}

void ExtensionInfoBarGtk::StoppedShowing() {
  gtk_chrome_button_unset_paint_state(GTK_CHROME_BUTTON(button_));
}

Browser* ExtensionInfoBarGtk::GetBrowser() {
  // Get the Browser object this infobar is attached to.
  GtkWindow* parent = platform_util::GetTopLevel(icon_);
  if (!parent)
    return NULL;

  return BrowserWindowGtk::GetBrowserWindowForNativeWindow(parent)->browser();
}

ui::MenuModel* ExtensionInfoBarGtk::BuildMenuModel() {
  const Extension* extension = delegate_->extension();
  if (!extension->ShowConfigureContextMenus())
    return NULL;

  Browser* browser = GetBrowser();
  if (!browser)
    return NULL;

  return new ExtensionContextMenuModel(extension, browser, NULL);
}

void ExtensionInfoBarGtk::OnSizeAllocate(GtkWidget* widget,
                                         GtkAllocation* allocation) {
  gfx::Size new_size(allocation->width, allocation->height);

  delegate_->extension_host()->view()->render_view_host()->view()
      ->SetSize(new_size);
}

gboolean ExtensionInfoBarGtk::OnButtonPress(GtkWidget* widget,
                                            GdkEventButton* event) {
  if (event->button != 1)
    return FALSE;

  ui::MenuModel* model = BuildMenuModel();
  if (!model)
    return FALSE;

  gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(widget),
                                    GTK_STATE_ACTIVE);
  ShowMenuWithModel(widget, this, model);

  return TRUE;
}

gboolean ExtensionInfoBarGtk::OnExpose(GtkWidget* sender,
                                       GdkEventExpose* event) {
  // We also need to draw our infobar arrows over the renderer.
  static_cast<InfoBarContainerGtk*>(container())->
      PaintInfobarBitsOn(sender, event, this);

  return FALSE;
}

InfoBar* ExtensionInfoBarDelegate::CreateInfoBar(InfoBarTabHelper* owner) {
  return new ExtensionInfoBarGtk(owner, this);
}
