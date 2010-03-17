// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/infobars/extension_infobar.h"

#include "app/gfx/canvas.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "chrome/browser/extensions/extension_action_context_menu_model.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/common/extensions/extension.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu_2.h"

// The horizontal margin between the menu and the Extension (HTML) view.
static const int kMenuHorizontalMargin = 1;

// The amount of space to the right of the Extension (HTML) view (to avoid
// overlapping the close button for the InfoBar).
static const int kFarRightMargin = 30;

// The margin between the extension icon and the drop-down arrow bitmap.
static const int kDropArrowLeftMargin = 3;

ExtensionInfoBar::ExtensionInfoBar(ExtensionInfoBarDelegate* delegate)
    : InfoBar(delegate),
      delegate_(delegate) {
  ExtensionHost* extension_host = delegate_->extension_host();

  // We set the target height for the InfoBar to be the height of the
  // ExtensionView it contains (plus 1 because the view should not overlap the
  // separator line at the bottom). When the InfoBar is first created, however,
  // this value is 0 but becomes positive after the InfoBar has been shown. See
  // function: OnExtensionPreferredSizeChanged.
  gfx::Size sz = extension_host->view()->GetPreferredSize();
  if (sz.height() > 0)
    sz.set_height(sz.height() + 1);
  set_target_height(sz.height());

  // Setup the extension icon and its associated drop down menu.
  SetupIconAndMenu();

  // Get notified of resize events for the ExtensionView.
  extension_host->view()->SetContainer(this);
  // We show the ExtensionView, but we don't want it deleted when we get
  // destroyed, which happens on tab switching (for example).
  extension_host->view()->set_parent_owned(false);
  AddChildView(extension_host->view());
}

ExtensionInfoBar::~ExtensionInfoBar() {
}

void ExtensionInfoBar::OnExtensionPreferredSizeChanged(ExtensionView* view) {
  DCHECK(view == delegate_->extension_host()->view());
  delegate_->extension_host()->view()->SetVisible(true);

  gfx::Size sz = view->GetPreferredSize();
  // Clamp height to a min and a max size of between 1 and 2 InfoBars.
  int default_height = static_cast<int>(InfoBar::kDefaultTargetHeight);
  sz.set_height(std::max(default_height, sz.height()));
  sz.set_height(std::min(2 * default_height, sz.height()));

  if (height() == 0)
    animation()->Reset(0.0);
  set_target_height(sz.height());
  animation()->Show();
}

void ExtensionInfoBar::Layout() {
  // Layout the close button and the background.
  InfoBar::Layout();

  // Layout the extension icon + drop down menu.
  int x = 0;
  gfx::Size sz = menu_->GetPreferredSize();
  menu_->SetBounds(x,
                  (height() - sz.height()) / 2,
                   sz.width(), sz.height());
  x += sz.width() + kMenuHorizontalMargin;

  // Layout the ExtensionView, showing the HTML InfoBar.
  ExtensionView* view = delegate_->extension_host()->view();
  view->SetBounds(x, 0, width() - x - kFarRightMargin - 1, height() - 1);
}

void ExtensionInfoBar::RunMenu(View* source, const gfx::Point& pt) {
  if (!options_menu_contents_.get())
    options_menu_contents_.reset(new ExtensionActionContextMenuModel(
        delegate_->extension_host()->extension(),
        // Do not include "Inspect Popup" in menu:
        NULL,  // ExtensionAction
        NULL,  // PrefService
        NULL));  // ExtensionActionContextMenuModel::MenuDelegate

  options_menu_menu_.reset(new views::Menu2(options_menu_contents_.get()));
  options_menu_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPLEFT);
}

void ExtensionInfoBar::SetupIconAndMenu() {
  SkBitmap icon;

  ExtensionResource icon_resource;
  Extension::Icons size = delegate_->extension_host()->extension()->
      GetIconPathAllowLargerSize(&icon_resource,
                                 Extension::EXTENSION_ICON_BITTY);
  if (!icon_resource.GetFilePath().empty()) {
    scoped_ptr<SkBitmap> bitmap;
    Extension::DecodeIconFromPath(icon_resource.GetFilePath(), size, &bitmap);
    if (bitmap.get())
      icon = *bitmap.release();
  }

  // Failure to get the path or failure to decode it causes fall-back to the
  // default extension icon.
  if (icon.empty()) {
    icon = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_EXTENSION_DEFAULT_ICON);
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap* drop_image = rb.GetBitmapNamed(IDR_APP_DROPARROW);
  menu_ = new views::MenuButton(NULL, std::wstring(), this, false);

  int image_size = Extension::EXTENSION_ICON_BITTY;
  scoped_ptr<gfx::Canvas> canvas(
      new gfx::Canvas(image_size + kDropArrowLeftMargin + drop_image->width(),
                      image_size, false));
  canvas->DrawBitmapInt(icon,
                        0, 0, icon.width(), icon.height(),
                        0, 0, image_size, image_size,
                        false);
  canvas->DrawBitmapInt(*drop_image,
                        image_size + kDropArrowLeftMargin,
                        image_size / 2);
  menu_->SetIcon(canvas->ExtractBitmap());

  AddChildView(menu_);
}

InfoBar* ExtensionInfoBarDelegate::CreateInfoBar() {
  return new ExtensionInfoBar(this);
}
