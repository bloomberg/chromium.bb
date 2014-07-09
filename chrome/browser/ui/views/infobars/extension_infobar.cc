// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/extension_infobar.h"

#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"


// ExtensionInfoBarDelegate ----------------------------------------------------

// static
scoped_ptr<infobars::InfoBar> ExtensionInfoBarDelegate::CreateInfoBar(
    scoped_ptr<ExtensionInfoBarDelegate> delegate) {
  Browser* browser = delegate->browser_;
  return scoped_ptr<infobars::InfoBar>(
      new ExtensionInfoBar(delegate.Pass(), browser));
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

ExtensionInfoBar::ExtensionInfoBar(
    scoped_ptr<ExtensionInfoBarDelegate> delegate,
    Browser* browser)
    : InfoBarView(delegate.PassAs<infobars::InfoBarDelegate>()),
      browser_(browser),
      infobar_icon_(NULL),
      icon_as_menu_(NULL),
      icon_as_image_(NULL),
      weak_ptr_factory_(this) {
}

ExtensionInfoBar::~ExtensionInfoBar() {
}

void ExtensionInfoBar::Layout() {
  InfoBarView::Layout();

  infobar_icon_->SetPosition(gfx::Point(StartX(), OffsetY(infobar_icon_)));
  ExtensionViewViews* extension_view = GetExtensionView();
  // TODO(pkasting): We'd like to simply set the extension view's desired height
  // at creation time and position using OffsetY() like for other infobar items,
  // but the NativeViewHost inside does not seem to be clipped by the ClipRect()
  // call in InfoBarView::PaintChildren(), so we have to manually clamp the size
  // here.
  extension_view->SetSize(
      gfx::Size(std::max(0, EndX() - StartX() - NonExtensionViewWidth()),
                std::min(height() - kSeparatorLineHeight - arrow_height(),
                         GetDelegate()->height())));
  // We do SetPosition() separately after SetSize() so OffsetY() will work.
  extension_view->SetPosition(
      gfx::Point(infobar_icon_->bounds().right() + kIconHorizontalMargin,
                 std::max(arrow_height(), OffsetY(extension_view))));
}

void ExtensionInfoBar::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (!details.is_add || (details.child != this) || (infobar_icon_ != NULL)) {
    InfoBarView::ViewHierarchyChanged(details);
    return;
  }

  extensions::ExtensionViewHost* extension_view_host =
      GetDelegate()->extension_view_host();

  if (extension_view_host->extension()->ShowConfigureContextMenus()) {
    icon_as_menu_ = new views::MenuButton(NULL, base::string16(), this, false);
    icon_as_menu_->SetFocusable(true);
    infobar_icon_ = icon_as_menu_;
  } else {
    icon_as_image_ = new views::ImageView();
    infobar_icon_ = icon_as_image_;
  }

  // Wait until the icon image is loaded before showing it.
  infobar_icon_->SetVisible(false);
  AddChildView(infobar_icon_);

  // Set the desired height of the ExtensionViewViews, so that when the
  // AddChildView() call triggers InfoBarView::ViewHierarchyChanged(), it can
  // read the correct height off this object in order to calculate the overall
  // desired infobar height.
  GetExtensionView()->SetSize(gfx::Size(0, GetDelegate()->height()));
  AddChildView(GetExtensionView());

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  InfoBarView::ViewHierarchyChanged(details);

  // This must happen after adding all children because it can trigger layout,
  // which assumes that particular children (e.g. the close button) have already
  // been added.
  const extensions::Extension* extension = extension_view_host->extension();
  extension_misc::ExtensionIcons image_size =
      extension_misc::EXTENSION_ICON_BITTY;
  extensions::ExtensionResource icon_resource =
      extensions::IconsInfo::GetIconResource(
          extension, image_size, ExtensionIconSet::MATCH_EXACTLY);
  extensions::ImageLoader* loader =
      extensions::ImageLoader::Get(extension_view_host->browser_context());
  loader->LoadImageAsync(
      extension,
      icon_resource,
      gfx::Size(image_size, image_size),
      base::Bind(&ExtensionInfoBar::OnImageLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

int ExtensionInfoBar::ContentMinimumWidth() const {
  return NonExtensionViewWidth() + static_cast<const ExtensionViewViews*>(
      GetDelegate()->extension_view_host()->view())->GetMinimumSize().width();
}

void ExtensionInfoBar::OnMenuButtonClicked(views::View* source,
                                           const gfx::Point& point) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  const extensions::Extension* extension =
      GetDelegate()->extension_view_host()->extension();
  DCHECK(icon_as_menu_);

  scoped_refptr<ExtensionContextMenuModel> options_menu_contents =
      new ExtensionContextMenuModel(extension, browser_);
  DCHECK_EQ(icon_as_menu_, source);
  RunMenuAt(
      options_menu_contents.get(), icon_as_menu_, views::MENU_ANCHOR_TOPLEFT);
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
    icon_as_menu_->SetImage(views::Button::STATE_NORMAL, menu_image);
  } else {
    icon_as_image_->SetImage(*icon);
  }

  infobar_icon_->SizeToPreferredSize();
  infobar_icon_->SetVisible(true);

  Layout();
}

ExtensionInfoBarDelegate* ExtensionInfoBar::GetDelegate() {
  return delegate()->AsExtensionInfoBarDelegate();
}

const ExtensionInfoBarDelegate* ExtensionInfoBar::GetDelegate() const {
  return delegate()->AsExtensionInfoBarDelegate();
}

ExtensionViewViews* ExtensionInfoBar::GetExtensionView() {
  return static_cast<ExtensionViewViews*>(
      GetDelegate()->extension_view_host()->view());
}

int ExtensionInfoBar::NonExtensionViewWidth() const {
  return infobar_icon_->width() + kIconHorizontalMargin;
}
