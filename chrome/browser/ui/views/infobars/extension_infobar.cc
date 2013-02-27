// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/extension_infobar.h"

#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "grit/theme_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/widget/widget.h"

// ExtensionInfoBarDelegate ----------------------------------------------------

InfoBar* ExtensionInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  return new ExtensionInfoBar(browser_, owner, this);
}

// ExtensionInfoBar ------------------------------------------------------------

namespace {
// The horizontal margin between the infobar icon and the Extension (HTML) view.
const int kIconHorizontalMargin = 1;

class MenuImageSource: public gfx::CanvasImageSource {
 public:
  MenuImageSource(const gfx::ImageSkia& icon, const gfx::ImageSkia& drop_image)
      : gfx::CanvasImageSource(ComputeSize(drop_image), false),
        icon_(icon),
        drop_image_(drop_image) {
  }

  virtual ~MenuImageSource() {
  }

  // Overridden from gfx::CanvasImageSource
  virtual void Draw(gfx::Canvas* canvas) OVERRIDE {
    int image_size = extension_misc::EXTENSION_ICON_BITTY;
    canvas->DrawImageInt(icon_, 0, 0, icon_.width(), icon_.height(), 0, 0,
                         image_size, image_size, false);
    canvas->DrawImageInt(drop_image_, image_size + kDropArrowLeftMargin,
                         image_size / 2);
  }

 private:
  gfx::Size ComputeSize(const gfx::ImageSkia& drop_image) const {
    int image_size = extension_misc::EXTENSION_ICON_BITTY;
    return gfx::Size(image_size + kDropArrowLeftMargin + drop_image.width(),
                     image_size);
  }

  // The margin between the extension icon and the drop-down arrow image.
  static const int kDropArrowLeftMargin = 3;

  const gfx::ImageSkia icon_;
  const gfx::ImageSkia drop_image_;

  DISALLOW_COPY_AND_ASSIGN(MenuImageSource);
};

}  // namespace

ExtensionInfoBar::ExtensionInfoBar(Browser* browser,
                                   InfoBarService* owner,
                                   ExtensionInfoBarDelegate* delegate)
    : InfoBarView(owner, delegate),
      delegate_(delegate),
      browser_(browser),
      infobar_icon_(NULL),
      icon_as_menu_(NULL),
      icon_as_image_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
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

  gfx::Size size = infobar_icon_->GetPreferredSize();
  infobar_icon_->SetBounds(StartX(), OffsetY(size), size.width(),
                           size.height());

  GetDelegate()->extension_host()->view()->SetBounds(
      infobar_icon_->bounds().right() + kIconHorizontalMargin,
      arrow_height(),
      std::max(0, EndX() - StartX() - ContentMinimumWidth()),
      height() - arrow_height() - 1);
}

void ExtensionInfoBar::ViewHierarchyChanged(bool is_add,
                                            views::View* parent,
                                            views::View* child) {
  if (!is_add || (child != this) || (infobar_icon_ != NULL)) {
    InfoBarView::ViewHierarchyChanged(is_add, parent, child);
    return;
  }

  extensions::ExtensionHost* extension_host = GetDelegate()->extension_host();

  if (extension_host->extension()->ShowConfigureContextMenus()) {
    icon_as_menu_ = new views::MenuButton(NULL, string16(), this, false);
    icon_as_menu_->set_focusable(true);
    infobar_icon_ = icon_as_menu_;
  } else {
    icon_as_image_ = new views::ImageView();
    infobar_icon_ = icon_as_image_;
  }

  // Wait until the icon image is loaded before showing it.
  infobar_icon_->SetVisible(false);
  AddChildView(infobar_icon_);

  AddChildView(extension_host->view());

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  InfoBarView::ViewHierarchyChanged(is_add, parent, child);

  // This must happen after adding all children because it can trigger layout,
  // which assumes that particular children (e.g. the close button) have already
  // been added.
  const extensions::Extension* extension = extension_host->extension();
  extension_misc::ExtensionIcons image_size =
      extension_misc::EXTENSION_ICON_BITTY;
  ExtensionResource icon_resource = extension->GetIconResource(
      image_size, ExtensionIconSet::MATCH_EXACTLY);
  extensions::ImageLoader* loader =
      extensions::ImageLoader::Get(extension_host->profile());
  loader->LoadImageAsync(
      extension,
      icon_resource,
      gfx::Size(image_size, image_size),
      base::Bind(&ExtensionInfoBar::OnImageLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

int ExtensionInfoBar::ContentMinimumWidth() const {
  return infobar_icon_->GetPreferredSize().width() + kIconHorizontalMargin;

}

void ExtensionInfoBar::OnDelegateDeleted() {
  delegate_ = NULL;
}

void ExtensionInfoBar::OnMenuButtonClicked(views::View* source,
                                           const gfx::Point& point) {
  if (!owned())
    return;  // We're closing; don't call anything, it might access the owner.
  const extensions::Extension* extension = GetDelegate()->extension_host()->
      extension();
  DCHECK(icon_as_menu_);

  scoped_refptr<ExtensionContextMenuModel> options_menu_contents =
      new ExtensionContextMenuModel(extension, browser_);
  DCHECK_EQ(icon_as_menu_, source);
  RunMenuAt(options_menu_contents.get(),
            icon_as_menu_,
            views::MenuItemView::TOPLEFT);
}

void ExtensionInfoBar::OnImageLoaded(const gfx::Image& image) {
  if (!GetDelegate())
    return;  // The delegate can go away while we asynchronously load images.

  const gfx::ImageSkia* icon = NULL;
  // Fall back on the default extension icon on failure.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (image.IsEmpty())
    icon = rb.GetImageNamed(IDR_EXTENSIONS_SECTION).ToImageSkia();
  else
    icon = image.ToImageSkia();

  if (icon_as_menu_) {
    const gfx::ImageSkia* drop_image =
        rb.GetImageNamed(IDR_APP_DROPARROW).ToImageSkia();

    gfx::CanvasImageSource* source = new MenuImageSource(*icon, *drop_image);
    gfx::ImageSkia menu_image = gfx::ImageSkia(source, source->size());
    icon_as_menu_->SetIcon(menu_image);
  } else {
    icon_as_image_->SetImage(*icon);
  }

  infobar_icon_->SetVisible(true);

  Layout();
}

ExtensionInfoBarDelegate* ExtensionInfoBar::GetDelegate() {
  return delegate_ ? delegate_->AsExtensionInfoBarDelegate() : NULL;
}
