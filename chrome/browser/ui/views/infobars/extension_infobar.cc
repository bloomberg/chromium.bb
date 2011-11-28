// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/extension_infobar.h"

#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "grit/theme_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/widget/widget.h"

// ExtensionInfoBarDelegate ----------------------------------------------------

InfoBar* ExtensionInfoBarDelegate::CreateInfoBar(InfoBarTabHelper* owner) {
  return new ExtensionInfoBar(browser_, owner, this);
}

// ExtensionInfoBar ------------------------------------------------------------

namespace {
// The horizontal margin between the menu and the Extension (HTML) view.
const int kMenuHorizontalMargin = 1;
}  // namespace

ExtensionInfoBar::ExtensionInfoBar(Browser* browser,
                                   InfoBarTabHelper* owner,
                                   ExtensionInfoBarDelegate* delegate)
    : InfoBarView(owner, delegate),
      delegate_(delegate),
      browser_(browser),
      menu_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)) {
  delegate->set_observer(this);

  int height = delegate->height();
  SetBarTargetHeight((height > 0) ? (height + kSeparatorLineHeight) : 0);
}

ExtensionInfoBar::~ExtensionInfoBar() {
  if (GetDelegate())
    GetDelegate()->set_observer(NULL);
}

void ExtensionInfoBar::Layout() {
  InfoBarView::Layout();

  gfx::Size menu_size = menu_->GetPreferredSize();
  menu_->SetBounds(StartX(), OffsetY(menu_size), menu_size.width(),
                   menu_size.height());

  GetDelegate()->extension_host()->view()->SetBounds(
      menu_->bounds().right() + kMenuHorizontalMargin,
      arrow_height(),
      std::max(0, EndX() - StartX() - ContentMinimumWidth()),
      height() - arrow_height() - 1);
}

void ExtensionInfoBar::ViewHierarchyChanged(bool is_add,
                                            View* parent,
                                            View* child) {
  if (!is_add || (child != this) || (menu_ != NULL)) {
    InfoBarView::ViewHierarchyChanged(is_add, parent, child);
    return;
  }

  menu_ = new views::MenuButton(NULL, string16(), this, false);
  menu_->SetVisible(false);
  menu_->set_focusable(true);
  AddChildView(menu_);

  ExtensionHost* extension_host = GetDelegate()->extension_host();
  AddChildView(extension_host->view());

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  InfoBarView::ViewHierarchyChanged(is_add, parent, child);

  // This must happen after adding all children because it can trigger layout,
  // which assumes that particular children (e.g. the close button) have already
  // been added.
  const Extension* extension = extension_host->extension();
  int image_size = Extension::EXTENSION_ICON_BITTY;
  ExtensionResource icon_resource = extension->GetIconResource(
      image_size, ExtensionIconSet::MATCH_EXACTLY);
  if (!icon_resource.relative_path().empty()) {
    tracker_.LoadImage(extension, icon_resource,
        gfx::Size(image_size, image_size), ImageLoadingTracker::DONT_CACHE);
  } else {
    OnImageLoaded(NULL, icon_resource, 0);
  }
}

int ExtensionInfoBar::ContentMinimumWidth() const {
  return menu_->GetPreferredSize().width() + kMenuHorizontalMargin;
}

void ExtensionInfoBar::OnImageLoaded(SkBitmap* image,
                                     const ExtensionResource& resource,
                                     int index) {
  if (!GetDelegate())
    return;  // The delegate can go away while we asynchronously load images.

  SkBitmap* icon = image;
  // Fall back on the default extension icon on failure.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (!image || image->empty())
    icon = rb.GetBitmapNamed(IDR_EXTENSIONS_SECTION);

  SkBitmap* drop_image = rb.GetBitmapNamed(IDR_APP_DROPARROW);

  int image_size = Extension::EXTENSION_ICON_BITTY;
  // The margin between the extension icon and the drop-down arrow bitmap.
  static const int kDropArrowLeftMargin = 3;
  scoped_ptr<gfx::CanvasSkia> canvas(new gfx::CanvasSkia(
      image_size + kDropArrowLeftMargin + drop_image->width(), image_size,
      false));
  canvas->DrawBitmapInt(*icon, 0, 0, icon->width(), icon->height(), 0, 0,
                        image_size, image_size, false);
  canvas->DrawBitmapInt(*drop_image, image_size + kDropArrowLeftMargin,
                        image_size / 2);
  menu_->SetIcon(canvas->ExtractBitmap());
  menu_->SetVisible(true);

  Layout();
}

void ExtensionInfoBar::OnDelegateDeleted() {
  delegate_ = NULL;
}

void ExtensionInfoBar::RunMenu(View* source, const gfx::Point& pt) {
  if (!owned())
    return;  // We're closing; don't call anything, it might access the owner.
  const Extension* extension = GetDelegate()->extension_host()->extension();
  if (!extension->ShowConfigureContextMenus())
    return;

  scoped_refptr<ExtensionContextMenuModel> options_menu_contents =
      new ExtensionContextMenuModel(extension, browser_, NULL);
  DCHECK_EQ(source, menu_);
  RunMenuAt(options_menu_contents.get(), menu_, views::MenuItemView::TOPLEFT);
}

ExtensionInfoBarDelegate* ExtensionInfoBar::GetDelegate() {
  return delegate_ ? delegate_->AsExtensionInfoBarDelegate() : NULL;
}
