// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extension_infobar_gtk.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"

ExtensionInfoBarGtk::ExtensionInfoBarGtk(ExtensionInfoBarDelegate* delegate)
    : InfoBar(delegate),
      tracker_(this),
      delegate_(delegate),
      view_(NULL) {
  delegate_->extension_host()->view()->SetContainer(this);
  BuildWidgets();
}

ExtensionInfoBarGtk::~ExtensionInfoBarGtk() {
  // This view is not owned by us, so unparent.
  gtk_widget_unparent(view_->native_view());
}

void ExtensionInfoBarGtk::OnImageLoaded(
    SkBitmap* image, ExtensionResource resource, int index) {
  if (!delegate_)
    return;  // The delegate can go away while we asynchronously load images.

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  SkBitmap* icon;
  if (!image || image->empty())
    icon = rb.GetBitmapNamed(IDR_EXTENSIONS_SECTION);
  else
    icon = image;

  // TODO(finnur): We now have the icon for the menu button, show the menu
  // button and layout.
}

void ExtensionInfoBarGtk::BuildWidgets() {
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
    OnImageLoaded(NULL, icon_resource, 0);  // |image|, |index|.
  }

  ExtensionHost* extension_host = delegate_->extension_host();
  view_ = extension_host->view();
  if (gtk_widget_get_parent(view_->native_view())) {
    gtk_widget_reparent(view_->native_view(), hbox_);
    gtk_box_set_child_packing(GTK_BOX(hbox_), view_->native_view(),
                              TRUE, TRUE, 0, GTK_PACK_START);
  } else {
    gtk_box_pack_start(GTK_BOX(hbox_), view_->native_view(), TRUE, TRUE, 0);
  }

  g_signal_connect(view_->native_view(), "size_allocate",
                   G_CALLBACK(&OnSizeAllocateThunk), this);
}

void ExtensionInfoBarGtk::OnSizeAllocate(GtkWidget* widget,
                                         GtkAllocation* allocation) {
  gfx::Size new_size(allocation->width, allocation->height);

  delegate_->extension_host()->view()->render_view_host()->view()
      ->SetSize(new_size);
}

void ExtensionInfoBarGtk::OnExtensionPreferredSizeChanged(
    ExtensionViewGtk* view,
    const gfx::Size& new_size) {
  // TODO(rafaelw) - Size the InfobarGtk vertically based on the preferred size
  // of the content.
}

InfoBar* ExtensionInfoBarDelegate::CreateInfoBar() {
  return new ExtensionInfoBarGtk(this);
}
