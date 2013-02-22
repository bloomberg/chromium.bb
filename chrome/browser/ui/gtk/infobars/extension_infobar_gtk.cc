// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/extension_infobar_gtk.h"

#include "base/debug/trace_event.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/infobars/infobar_container_gtk.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

ExtensionInfoBarGtk::ExtensionInfoBarGtk(InfoBarService* owner,
                                         ExtensionInfoBarDelegate* delegate)
    : InfoBarGtk(owner, delegate),
      delegate_(delegate),
      view_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  delegate->set_observer(this);

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

ExtensionInfoBarGtk::~ExtensionInfoBarGtk() {
  if (delegate_)
    delegate_->set_observer(NULL);
}

void ExtensionInfoBarGtk::PlatformSpecificHide(bool animate) {
  // This view is not owned by us; we can't unparent it because we aren't the
  // owning container.
  gtk_util::RemoveAllChildren(alignment_);
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

void ExtensionInfoBarGtk::OnImageLoaded(const gfx::Image& image) {
  if (!delegate_)
    return;  // The delegate can go away while we asynchronously load images.

  // TODO(erg): IDR_EXTENSIONS_SECTION should have an IDR_INFOBAR_EXTENSIONS
  // icon of the correct size with real subpixel shading and such.
  const gfx::ImageSkia* icon = NULL;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (image.IsEmpty())
    icon = rb.GetImageSkiaNamed(IDR_EXTENSIONS_SECTION);
  else
    icon = image.ToImageSkia();

  gfx::ImageSkia* drop_image = rb.GetImageSkiaNamed(IDR_APP_DROPARROW);

  int image_size = extension_misc::EXTENSION_ICON_BITTY;
  // The margin between the extension icon and the drop-down arrow bitmap.
  static const int kDropArrowLeftMargin = 3;
  scoped_ptr<gfx::Canvas> canvas(new gfx::Canvas(
      gfx::Size(image_size + kDropArrowLeftMargin + drop_image->width(),
                image_size), ui::SCALE_FACTOR_100P, false));
  canvas->DrawImageInt(*icon, 0, 0, icon->width(), icon->height(), 0, 0,
                       image_size, image_size, false);
  canvas->DrawImageInt(*drop_image, image_size + kDropArrowLeftMargin,
                       image_size / 2);

  SkBitmap bitmap = canvas->ExtractImageRep().sk_bitmap();
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(bitmap);
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
  const extensions::Extension* extension =
      delegate_->extension_host()->extension();
  ExtensionResource icon_resource = extension->GetIconResource(
      extension_misc::EXTENSION_ICON_BITTY, ExtensionIconSet::MATCH_EXACTLY);
  // Load image asynchronously, calling back OnImageLoaded.
  extensions::ImageLoader* loader =
      extensions::ImageLoader::Get(delegate_->extension_host()->profile());
  loader->LoadImageAsync(extension, icon_resource,
                         gfx::Size(extension_misc::EXTENSION_ICON_BITTY,
                                   extension_misc::EXTENSION_ICON_BITTY),
                         base::Bind(&ExtensionInfoBarGtk::OnImageLoaded,
                                    weak_ptr_factory_.GetWeakPtr()));

  // Pad the bottom of the infobar by one pixel for the border.
  alignment_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment_), 0, 1, 0, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), alignment_, TRUE, TRUE, 0);

  extensions::ExtensionHost* extension_host = delegate_->extension_host();
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

void ExtensionInfoBarGtk::OnDelegateDeleted() {
  delegate_ = NULL;
}

Browser* ExtensionInfoBarGtk::GetBrowser() {
  // Get the Browser object this infobar is attached to.
  GtkWindow* parent = platform_util::GetTopLevel(icon_);
  if (!parent)
    return NULL;

  return BrowserWindowGtk::GetBrowserWindowForNativeWindow(parent)->browser();
}

ExtensionContextMenuModel* ExtensionInfoBarGtk::BuildMenuModel() {
  const extensions::Extension* extension = delegate_->extension();
  if (!extension->ShowConfigureContextMenus())
    return NULL;

  Browser* browser = GetBrowser();
  if (!browser)
    return NULL;

  return new ExtensionContextMenuModel(extension, browser);
}

void ExtensionInfoBarGtk::OnSizeAllocate(GtkWidget* widget,
                                         GtkAllocation* allocation) {
  gfx::Size new_size(allocation->width, allocation->height);

  delegate_->extension_host()->view()->render_view_host()->GetView()
      ->SetSize(new_size);
}

gboolean ExtensionInfoBarGtk::OnButtonPress(GtkWidget* widget,
                                            GdkEventButton* event) {
  if (event->button != 1)
    return FALSE;

  context_menu_model_ = BuildMenuModel();
  if (!context_menu_model_)
    return FALSE;

  gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(widget),
                                    GTK_STATE_ACTIVE);
  ShowMenuWithModel(widget, this, context_menu_model_);

  return TRUE;
}

gboolean ExtensionInfoBarGtk::OnExpose(GtkWidget* sender,
                                       GdkEventExpose* event) {
  TRACE_EVENT0("ui::gtk", "ExtensionInfoBarGtk::OnExpose");

  // We also need to draw our infobar arrows over the renderer.
  static_cast<InfoBarContainerGtk*>(container())->
      PaintInfobarBitsOn(sender, event, this);

  return FALSE;
}

InfoBar* ExtensionInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  return new ExtensionInfoBarGtk(owner, this);
}
